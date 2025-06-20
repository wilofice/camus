// =================================================================
// src/Camus/LlmInteraction.cpp
// =================================================================
// Real implementation using llama.cpp - API has been corrected

#include "Camus/LlmInteraction.hpp"
#include "llama.h"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <thread>
#include <string>

namespace Camus {

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
    // The C-style API requires us to manage the token buffer.
    // We create a vector that is definitely large enough.
    std::vector<llama_token> tokens_list;
    tokens_list.resize(prompt.size());

    int n_tokens = llama_tokenize(
        m_model, 
        prompt.c_str(), 
        static_cast<int>(prompt.size()), 
        tokens_list.data(), 
        tokens_list.size(), 
        true,  // add BOS token
        false  // special tokens
    );

    if (n_tokens < 0) {
        throw std::runtime_error("Failed to tokenize prompt.");
    }
    tokens_list.resize(n_tokens);

    // Check if the prompt is too long for the context
    if (static_cast<uint32_t>(n_tokens) > llama_n_ctx(m_context)) {
        throw std::runtime_error("Prompt is too long for the model's context window.");
    }
    
    // Clear the KV cache and evaluate the initial prompt
    llama_kv_cache_clear(m_context);
    if (llama_decode(m_context, llama_batch_get_one(tokens_list.data(), n_tokens, 0, 0))) {
        throw std::runtime_error("Failed to decode prompt.");
    }
    
    std::string result;
    int n_cur = n_tokens;
    const int max_new_tokens = 4096;
    
    // Main generation loop
    while (n_cur <= max_new_tokens) {
        // Sample the next token
        auto* logits  = llama_get_logits_ith(m_context, 0);
        auto* candidates = new llama_token_data[llama_n_vocab(m_model)];
        llama_token_data_array candidates_p = { candidates, static_cast<size_t>(llama_n_vocab(m_model)), false };

        for (int token_id = 0; token_id < llama_n_vocab(m_model); token_id++) {
            candidates[token_id].id = token_id;
            candidates[token_id].logit = logits[token_id];
            candidates[token_id].p = 0.0f;
        }

        llama_token new_token_id = llama_sample_token_greedy(m_context, &candidates_p);
        delete[] candidates;

        // Break if we hit the end-of-sequence token
        if (new_token_id == llama_token_eos(m_model)) {
            break;
        }

        // Append the token string to the result
        char piece[8] = {0};
        llama_token_to_piece(m_model, new_token_id, piece, sizeof(piece), false);
        result += piece;
        std::cout << piece << std::flush;

        // Prepare for the next iteration
        if (llama_decode(m_context, llama_batch_get_one(&new_token_id, 1, n_cur, 0))) {
            throw std::runtime_error("Failed to decode generated token.");
        }
        n_cur++;
    }

    std::cout << std::endl;
    return result;
}

} // namespace Camus
