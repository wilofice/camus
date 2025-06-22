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

} // namespace Camus
