// =================================================================
// src/Camus/LlamaCppInteraction.cpp
// =================================================================
#include "Camus/LlamaCppInteraction.hpp"
#include "llama.h"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace Camus {

static void clean_llm_output(std::string& output) {
    output.erase(output.begin(), std::find_if(output.begin(), output.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));

    output.erase(std::find_if(output.rbegin(), output.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), output.end());

    if (output.size() > 3 && output.substr(0, 3) == "```") {
        size_t first_newline = output.find('\n');
        if (first_newline != std::string::npos) {
            output = output.substr(first_newline + 1);
        }
    }
    if (output.size() > 3 && output.substr(output.size() - 3) == "```") {
        output = output.substr(0, output.size() - 3);
    }

    output.erase(output.begin(), std::find_if(output.begin(), output.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    output.erase(std::find_if(output.rbegin(), output.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), output.end());
}


LlamaCppInteraction::LlamaCppInteraction(const std::string& model_path) : m_model_path(model_path) {
    llama_backend_init();

    auto mparams = llama_model_default_params();

    // --- GPU OFFLOAD SETTING ---
    // Set n_gpu_layers to a high number to offload as many layers as possible to the GPU.
    mparams.n_gpu_layers = 99;

    auto cparams = llama_context_default_params();
    cparams.n_ctx = 4096;
    cparams.n_threads = std::thread::hardware_concurrency();
    cparams.n_threads_batch = std::thread::hardware_concurrency();

    m_model = llama_load_model_from_file(model_path.c_str(), mparams);
    if (m_model == nullptr) {
        throw std::runtime_error("Failed to load model from path: " + model_path);
    }

    m_context = llama_new_context_with_model(m_model, cparams);
    if (m_context == nullptr) {
        llama_free_model(m_model);
        throw std::runtime_error("Failed to create llama context.");
    }

    // Initialize default metadata
    initializeDefaultMetadata();
    m_last_health_check = std::chrono::system_clock::now();
    performHealthCheck();

    std::cout << "[INFO] Successfully loaded model: " << model_path << std::endl;
}

LlamaCppInteraction::~LlamaCppInteraction() {
    if (m_context) llama_free(m_context);
    if (m_model) llama_free_model(m_model);
    llama_backend_free();
    std::cout << "[INFO] Cleaned up llama.cpp resources." << std::endl;
}

std::string LlamaCppInteraction::getCompletion(const std::string& prompt) {
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

    std::vector<llama_token> last_n_tokens;
    last_n_tokens.reserve(llama_n_ctx(m_context));
    last_n_tokens.insert(last_n_tokens.end(), tokens_list.begin(), tokens_list.end());

    const llama_token eot_token = llama_token_eot(m_model);

    while (n_generated < max_new_tokens) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
        if (elapsed_seconds > timeout_seconds) {
            throw std::runtime_error("Model generation timed out after " + std::to_string(timeout_seconds) + " seconds.");
        }

        auto* logits  = llama_get_logits_ith(m_context, 0);
        auto* candidates = new llama_token_data[llama_n_vocab(m_model)];
        llama_token_data_array candidates_p = { candidates, static_cast<size_t>(llama_n_vocab(m_model)), false };

        for (int token_id = 0; token_id < llama_n_vocab(m_model); token_id++) {
            candidates[token_id].id = token_id;
            candidates[token_id].logit = logits[token_id];
            candidates[token_id].p = 0.0f;
        }

        llama_sample_repetition_penalties(m_context, &candidates_p, last_n_tokens.data(), last_n_tokens.size(), 1.1f, 64, 1.0f);
        llama_sample_top_k(m_context, &candidates_p, 40, 1);
        llama_sample_top_p(m_context, &candidates_p, 0.95f, 1);
        llama_sample_temp(m_context, &candidates_p, 0.4f);

        llama_token new_token_id = llama_sample_token(m_context, &candidates_p);

        delete[] candidates;

        if (new_token_id == llama_token_eos(m_model) || new_token_id == eot_token) {
            break;
        }

        char piece[8] = {0};
        // Corrected the function call to include the 'special' boolean argument.
        llama_token_to_piece(m_model, new_token_id, piece, sizeof(piece), false);
        result += piece;
        std::cout << piece << std::flush;

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

    clean_llm_output(result);

    return result;
}

LlamaCppInteraction::LlamaCppInteraction(const std::string& model_path, const ModelMetadata& metadata) 
    : m_metadata(metadata), m_model_path(model_path) {
    llama_backend_init();

    auto mparams = llama_model_default_params();
    mparams.n_gpu_layers = 99;

    auto cparams = llama_context_default_params();
    cparams.n_ctx = metadata.performance.max_context_tokens;
    cparams.n_threads = std::thread::hardware_concurrency();
    cparams.n_threads_batch = std::thread::hardware_concurrency();

    m_model = llama_load_model_from_file(model_path.c_str(), mparams);
    if (m_model == nullptr) {
        throw std::runtime_error("Failed to load model from path: " + model_path);
    }

    m_context = llama_new_context_with_model(m_model, cparams);
    if (m_context == nullptr) {
        llama_free_model(m_model);
        throw std::runtime_error("Failed to create llama context.");
    }

    m_metadata.model_path = model_path;
    m_last_health_check = std::chrono::system_clock::now();
    performHealthCheck();
}

InferenceResponse LlamaCppInteraction::getCompletionWithMetadata(const InferenceRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    InferenceResponse response;
    response.text = getCompletion(request.prompt);
    
    auto end_time = std::chrono::steady_clock::now();
    response.response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    response.finish_reason = "stop";
    
    // Update performance metrics
    updatePerformanceMetrics(response);
    
    return response;
}

ModelMetadata LlamaCppInteraction::getModelMetadata() const {
    return m_metadata;
}

bool LlamaCppInteraction::isHealthy() const {
    return m_is_healthy && m_model != nullptr && m_context != nullptr;
}

bool LlamaCppInteraction::performHealthCheck() {
    m_last_health_check = std::chrono::system_clock::now();
    
    try {
        // Simple health check - verify model and context are valid
        if (m_model == nullptr || m_context == nullptr) {
            m_is_healthy = false;
            m_metadata.health_status_message = "Model or context is null";
            return false;
        }
        
        // Try a simple token encoding to verify model works
        const char* test_text = "test";
        std::vector<llama_token> tokens(10);  // Small buffer for test
        int n_tokens = llama_tokenize(m_model, test_text, 4, tokens.data(), tokens.size(), false, false);
        if (n_tokens <= 0) {
            m_is_healthy = false;
            m_metadata.health_status_message = "Failed to tokenize test string";
            return false;
        }
        
        m_is_healthy = true;
        m_metadata.is_healthy = true;
        m_metadata.is_available = true;
        m_metadata.health_status_message = "Healthy";
        m_metadata.last_health_check = m_last_health_check;
        
        return true;
    } catch (const std::exception& e) {
        m_is_healthy = false;
        m_metadata.health_status_message = "Health check failed: " + std::string(e.what());
        return false;
    }
}

ModelPerformance LlamaCppInteraction::getCurrentPerformance() const {
    return m_performance;
}

bool LlamaCppInteraction::warmUp() {
    try {
        // Perform a small test inference to warm up the model
        getCompletion("Hello");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void LlamaCppInteraction::cleanup() {
    if (m_context) {
        llama_free(m_context);
        m_context = nullptr;
    }
    if (m_model) {
        llama_free_model(m_model);
        m_model = nullptr;
    }
    m_is_healthy = false;
    m_metadata.is_healthy = false;
    m_metadata.is_available = false;
}

std::string LlamaCppInteraction::getModelId() const {
    if (m_metadata.name.empty()) {
        return generateModelId();
    }
    return m_metadata.name + "_" + m_metadata.version;
}

void LlamaCppInteraction::initializeDefaultMetadata() {
    if (m_metadata.name.empty()) {
        m_metadata.name = "LlamaCpp_Model";
    }
    if (m_metadata.description.empty()) {
        m_metadata.description = "Llama.cpp model instance";
    }
    if (m_metadata.version.empty()) {
        m_metadata.version = "unknown";
    }
    m_metadata.provider = "llama_cpp";
    m_metadata.model_path = m_model_path;
    
    // Set default capabilities if none specified
    if (m_metadata.capabilities.empty()) {
        m_metadata.capabilities = {ModelCapability::CODE_SPECIALIZED, ModelCapability::INSTRUCTION_TUNED};
    }
    
    // Initialize performance defaults
    if (m_metadata.performance.max_context_tokens == 0) {
        m_metadata.performance.max_context_tokens = 4096;
    }
    if (m_metadata.performance.max_output_tokens == 0) {
        m_metadata.performance.max_output_tokens = 2048;
    }
}

void LlamaCppInteraction::updatePerformanceMetrics(const InferenceResponse& response) const {
    // Update average latency using exponential moving average
    if (m_performance.avg_latency.count() == 0) {
        m_performance.avg_latency = response.response_time;
    } else {
        auto new_latency = static_cast<double>(response.response_time.count());
        auto current_latency = static_cast<double>(m_performance.avg_latency.count());
        auto updated_latency = 0.8 * current_latency + 0.2 * new_latency;
        m_performance.avg_latency = std::chrono::milliseconds(static_cast<long>(updated_latency));
    }
    
    // Update tokens per second if we have token count
    if (response.tokens_generated > 0 && response.response_time.count() > 0) {
        double tokens_per_ms = static_cast<double>(response.tokens_generated) / response.response_time.count();
        double tokens_per_second = tokens_per_ms * 1000.0;
        
        if (m_performance.tokens_per_second == 0.0) {
            m_performance.tokens_per_second = tokens_per_second;
        } else {
            m_performance.tokens_per_second = 0.8 * m_performance.tokens_per_second + 0.2 * tokens_per_second;
        }
    }
}

std::string LlamaCppInteraction::generateModelId() const {
    // Generate ID from model path hash
    std::hash<std::string> hasher;
    auto hash = hasher(m_model_path);
    return "llamacpp_" + std::to_string(hash);
}

} // namespace Camus
