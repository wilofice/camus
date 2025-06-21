// =================================================================
// src/Camus/LlmInteraction.cpp
// =================================================================
// Real implementation using llama.cpp - Now with robust sampling to prevent loops

#include "Camus/LlmInteraction.hpp"
#include "llama.h"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <thread>
#include <string>
#include <chrono> // For timeout
#include <algorithm> // For std::find_if
#include <cctype> // for std::isspace

namespace Camus {

// Helper function to trim markdown code fences and language specifiers
static void clean_llm_output(std::string& output) {
    // Trim leading whitespace
    output.erase(output.begin(), std::find_if(output.begin(), output.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));

    // Trim trailing whitespace
    output.erase(std::find_if(output.rbegin(), output.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), output.end());

    // Remove starting ```language and trailing ```
    if (output.size() > 3 && output.substr(0, 3) == "```") {
        size_t first_newline = output.find('\n');
        if (first_newline != std::string::npos) {
            output = output.substr(first_newline + 1);
        }
    }
    if (output.size() > 3 && output.substr(output.size() - 3) == "```") {
        output = output.substr(0, output.size() - 3);
    }
    
    // Trim again after removing fences
    output.erase(output.begin(), std::find_if(output.begin(), output.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    output.erase(std::find_if(output.rbegin(), output.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), output.end());
}


LlmInteraction::LlmInteraction(const std::string& model_path) {
    // Initialize llama.cpp backend
    llama_backend_init();

    // Set model and context parameters
    auto mparams = llama_model_default_params();
    auto cparams = llama_context_default_params();
    cparams.n_ctx = 4096;
    cparams.n_threads = std::thread::hardware_concurrency();
    cparams.n_threads_batch = std::thread::hardware_concurrency();
    
    // Load the model
    m_model = llama_load_model_from_file(model_path.c_str(), mparams);
    if (m_model == nullptr) {
        throw std::runtime_error("Failed to load model from path: " + model_path);
    }

    // Create the context
    m_context = llama_new_context_with_model(m_model, cparams);
    if (m_context == nullptr) {
        llama_free_model(m_model);
        throw std::runtime_error("Failed to create llama context.");
    }

    std::cout << "[INFO] Successfully loaded model: " << model_path << std::endl;
}

LlmInteraction::~LlmInteraction() {
    if (m_context) {
        llama_free(m_context);
    }
    if (m_model) {
        llama_free_model(m_model);
    }
    llama_backend_free();
    std::cout << "[INFO] Cleaned up llama.cpp resources." << std::endl;
}

std::string LlmInteraction::getCompletion(const std::string& prompt) {
    std::vector<llama_token> tokens_list;
    tokens_list.resize(prompt.size());

    int n_tokens = llama_tokenize(
        m_model, prompt.c_str(), static_cast<int>(prompt.size()), 
        tokens_list.data(), tokens_list.size(), true, false
    );

    if (n_tokens < 0) throw std::runtime_error("Failed to tokenize prompt.");
    tokens_list.resize(n_tokens);

    if (static_cast<uint32_t>(n_tokens) > llama_n_ctx(m_context)) {
        throw std::runtime_error("Prompt is too long for the model's context window.");
    }
    
    llama_kv_cache_clear(m_context);
    if (llama_decode(m_context, llama_batch_get_one(tokens_list.data(), n_tokens, 0, 0))) {
        throw std::runtime_error("Failed to decode prompt.");
    }
    
    std::string result;
    int n_generated = 0;
    const int max_new_tokens = 4096;
    const long long timeout_seconds = 120;
    auto start_time = std::chrono::steady_clock::now();
    
    // --- Use a temporary vector to store recent tokens for repetition penalty ---
    std::vector<llama_token> last_n_tokens;
    const int n_keep = n_tokens;
    last_n_tokens.reserve(llama_n_ctx(m_context));
    last_n_tokens.insert(last_n_tokens.end(), tokens_list.begin() + n_keep, tokens_list.end());


    while (n_generated < max_new_tokens) {
        // --- Timeout Check ---
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
        if (elapsed_seconds > timeout_seconds) {
            throw std::runtime_error("Model generation timed out after " + std::to_string(timeout_seconds) + " seconds.");
        }

        // --- ROBUST SAMPLING ---
        auto* logits  = llama_get_logits_ith(m_context, 0);
        auto* candidates = new llama_token_data[llama_n_vocab(m_model)];
        llama_token_data_array candidates_p = { candidates, static_cast<size_t>(llama_n_vocab(m_model)), false };

        for (int token_id = 0; token_id < llama_n_vocab(m_model); token_id++) {
            candidates[token_id].id = token_id;
            candidates[token_id].logit = logits[token_id];
            candidates[token_id].p = 0.0f;
        }

        // Apply repetition penalty
        llama_sample_repetition_penalties(m_context, &candidates_p, last_n_tokens.data(), last_n_tokens.size(), 1.1f, 64, 1.0f);

        // Temperature sampling
        llama_sample_top_k(m_context, &candidates_p, 40, 1);
        llama_sample_top_p(m_context, &candidates_p, 0.95f, 1);
        llama_sample_temp(m_context, &candidates_p, 0.8f);
        
        // Final sampling decision
        llama_token new_token_id = llama_sample_token(m_context, &candidates_p);
        
        delete[] candidates;

        if (new_token_id == llama_token_eos(m_model)) break;

        char piece[8] = {0};
        llama_token_to_piece(m_model, new_token_id, piece, sizeof(piece), false);
        result += piece;
        std::cout << piece << std::flush;

        // Update context for next token
        last_n_tokens.push_back(new_token_id);
        if (last_n_tokens.size() > 64) {
            last_n_tokens.erase(last_n_tokens.begin());
        }

        if (llama_decode(m_context, llama_batch_get_one(&new_token_id, 1, n_tokens + n_generated, 0))) {
            throw std::runtime_error("Failed to decode generated token.");
        }
        n_generated++;
    }

    std::cout << std::endl;
    
    // --- Post-processing ---
    clean_llm_output(result);

    return result;
}

} // namespace Camus
