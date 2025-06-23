// =================================================================
// src/Camus/OllamaInteraction.cpp
// =================================================================
// Revised implementation for Ollama API interaction with streaming.

#include "Camus/OllamaInteraction.hpp"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <functional>

namespace Camus {

// Helper function to trim markdown code fences from the final output
static void clean_llm_output(std::string& output) {
    const std::string fence = "```";
    size_t first_fence_pos = output.find(fence);
    size_t last_fence_pos = output.rfind(fence);

    if (first_fence_pos != std::string::npos && last_fence_pos != std::string::npos && last_fence_pos > first_fence_pos) {
        // --- Logging Logic ---
        std::string header = output.substr(0, first_fence_pos);
        std::string footer = output.substr(last_fence_pos + fence.length());

        if (!header.empty() || !footer.empty()) {
            std::ofstream log_file(".camus/reasoning.log", std::ios_base::app);
            if (log_file.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto in_time_t = std::chrono::system_clock::to_time_t(now);
                log_file << "--- Log Entry: " << std::ctime(&in_time_t);
                if (!header.empty()) {
                    log_file << "Header (Pre-Code):" << std::endl << header << std::endl;
                }
                if (!footer.empty()) {
                    log_file << "Footer (Post-Code):" << std::endl << footer << std::endl;
                }
                log_file << "--- End of Entry ---" << std::endl << std::endl;
            }
        }

        // --- Code Extraction Logic ---
        size_t content_start_pos = output.find('\n', first_fence_pos);
        if (content_start_pos != std::string::npos) {
            output = output.substr(content_start_pos + 1, last_fence_pos - (content_start_pos + 1));
        }
    }

    // Final trim of any remaining whitespace
    auto trim_whitespace = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    };
    trim_whitespace(output);
}


OllamaInteraction::OllamaInteraction(const std::string& server_url, const std::string& model_name)
    : m_server_url(server_url), m_model_name(model_name) {
    std::cout << "[INFO] Configured Ollama client for server: " << server_url
              << " with model: " << model_name << std::endl;
    
    // Initialize default metadata
    initializeDefaultMetadata();
    m_last_health_check = std::chrono::system_clock::now();
    performHealthCheck();
}

OllamaInteraction::OllamaInteraction(const std::string& server_url, const std::string& model_name, const ModelMetadata& metadata)
    : m_server_url(server_url), m_model_name(model_name), m_metadata(metadata) {
    std::cout << "[INFO] Configured Ollama client for server: " << server_url
              << " with model: " << model_name << std::endl;
              
    m_metadata.provider = "ollama";
    m_metadata.model_path = server_url + "/" + model_name;
    m_last_health_check = std::chrono::system_clock::now();
    performHealthCheck();
}

std::string OllamaInteraction::getCompletion(const std::string& prompt) {
    try {
        // The httplib constructor handles URL parsing automatically.
        httplib::Client client(m_server_url.c_str());
        client.set_read_timeout(300); // 5 minutes for generation
        client.set_connection_timeout(30); // 30 seconds to connect

        // Create JSON request body with streaming enabled  
        nlohmann::json request_body = {
            {"model", m_model_name},
            {"prompt", prompt},
            {"stream", false} // Enable streaming
        };

        std::cout << "[INFO] Sending request to Ollama server (streaming mode off)..." << std::endl;

        // Since httplib's streaming POST is complex, we'll use a custom approach
        // First, let's use the simple POST API and process the streaming response manually
        
        // For true streaming, we need to handle the response in chunks
        // Ollama sends newline-delimited JSON when streaming is enabled
        httplib::Headers headers = {{"Content-Type", "application/json"}};
        
        std::string accumulated_response;
        
        // Make the POST request
        auto res = client.Post(
            "/api/generate",
            headers,
            request_body.dump(),
            "application/json"
        );

        // Error checking
        if (!res) {
            throw std::runtime_error("Failed to connect to Ollama server at " + m_server_url);
        }

        if (res->status != 200) {
            throw std::runtime_error("Ollama server returned error status: " +
                                   std::to_string(res->status) +
                                   " - " + res->body);
        }

        // Process the streaming response
        // When streaming is enabled, Ollama returns multiple JSON objects separated by newlines
        std::istringstream response_stream(res->body);
        std::string line;
        
        while (std::getline(response_stream, line)) {
            if (line.empty()) continue;
            
            try {
                auto json_chunk = nlohmann::json::parse(line);
                if (json_chunk.contains("response")) {
                    std::string chunk_text = json_chunk["response"];
                    std::cout << chunk_text << std::flush;
                    accumulated_response += chunk_text;
                }
            } catch (const nlohmann::json::exception& e) {
                // Ignore malformed JSON lines
            }
        }
        
        std::cout << std::endl;
        
        // Clean the output
        clean_llm_output(accumulated_response);
        return accumulated_response;

    } catch (const std::exception& e) {
        // Re-throw with a more specific context
        throw std::runtime_error("Error in Ollama interaction: " + std::string(e.what()));
    }
}

InferenceResponse OllamaInteraction::getCompletionWithMetadata(const InferenceRequest& request) {
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

ModelMetadata OllamaInteraction::getModelMetadata() const {
    return m_metadata;
}

bool OllamaInteraction::isHealthy() const {
    return m_is_healthy;
}

bool OllamaInteraction::performHealthCheck() {
    m_last_health_check = std::chrono::system_clock::now();
    
    try {
        // Try to connect to the Ollama server
        httplib::Client client(m_server_url.c_str());
        client.set_connection_timeout(10); // 10 seconds timeout for health check
        
        // Try a simple API call to check server health
        auto res = client.Get("/api/tags");
        
        if (!res) {
            m_is_healthy = false;
            m_metadata.health_status_message = "Cannot connect to Ollama server";
            return false;
        }
        
        if (res->status != 200) {
            m_is_healthy = false;
            m_metadata.health_status_message = "Ollama server returned error: " + std::to_string(res->status);
            return false;
        }
        
        // Check if our specific model is available
        try {
            auto json_response = nlohmann::json::parse(res->body);
            bool model_found = false;
            
            if (json_response.contains("models")) {
                for (const auto& model : json_response["models"]) {
                    if (model.contains("name") && model["name"] == m_model_name) {
                        model_found = true;
                        break;
                    }
                }
            }
            
            if (!model_found) {
                m_is_healthy = false;
                m_metadata.health_status_message = "Model '" + m_model_name + "' not found on server";
                return false;
            }
        } catch (const nlohmann::json::exception&) {
            // If we can't parse the response, assume server is working but model check failed
            m_is_healthy = false;
            m_metadata.health_status_message = "Cannot verify model availability";
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

ModelPerformance OllamaInteraction::getCurrentPerformance() const {
    return m_performance;
}

bool OllamaInteraction::warmUp() {
    try {
        // Perform a small test inference to warm up the model
        getCompletion("Hi");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void OllamaInteraction::cleanup() {
    // For Ollama, cleanup mainly involves marking as unavailable
    m_is_healthy = false;
    m_metadata.is_healthy = false;
    m_metadata.is_available = false;
}

std::string OllamaInteraction::getModelId() const {
    if (m_metadata.name.empty()) {
        return generateModelId();
    }
    return m_metadata.name + "_" + m_metadata.version;
}

void OllamaInteraction::initializeDefaultMetadata() {
    if (m_metadata.name.empty()) {
        m_metadata.name = "Ollama_" + m_model_name;
    }
    if (m_metadata.description.empty()) {
        m_metadata.description = "Ollama model: " + m_model_name;
    }
    if (m_metadata.version.empty()) {
        m_metadata.version = "latest";
    }
    m_metadata.provider = "ollama";
    m_metadata.model_path = m_server_url + "/" + m_model_name;
    
    // Set default capabilities if none specified
    if (m_metadata.capabilities.empty()) {
        m_metadata.capabilities = {ModelCapability::HIGH_QUALITY, ModelCapability::INSTRUCTION_TUNED};
    }
    
    // Initialize performance defaults
    if (m_metadata.performance.max_context_tokens == 0) {
        m_metadata.performance.max_context_tokens = 8192;
    }
    if (m_metadata.performance.max_output_tokens == 0) {
        m_metadata.performance.max_output_tokens = 4096;
    }
}

void OllamaInteraction::updatePerformanceMetrics(const InferenceResponse& response) const {
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

std::string OllamaInteraction::generateModelId() const {
    // Generate ID from server URL and model name
    std::hash<std::string> hasher;
    auto hash = hasher(m_server_url + "_" + m_model_name);
    return "ollama_" + std::to_string(hash);
}

std::string OllamaInteraction::makeHttpRequest(const std::string& endpoint, const std::string& payload) const {
    httplib::Client client(m_server_url.c_str());
    client.set_read_timeout(30);
    client.set_connection_timeout(10);
    
    httplib::Headers headers = {{"Content-Type", "application/json"}};
    
    auto res = client.Post(endpoint.c_str(), headers, payload, "application/json");
    
    if (!res) {
        throw std::runtime_error("Failed to connect to Ollama server");
    }
    
    if (res->status != 200) {
        throw std::runtime_error("HTTP error " + std::to_string(res->status) + ": " + res->body);
    }
    
    return res->body;
}

} // namespace Camus
