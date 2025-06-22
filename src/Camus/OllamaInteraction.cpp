// =================================================================
// src/Camus/OllamaInteraction.cpp
// =================================================================
// Revised implementation for Ollama API interaction with streaming.

#include "Camus/OllamaInteraction.hpp"
#define CPPHTTPLIB_OPENSSL_SUPPORT // Enable SSL for https if needed
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace Camus {

// Helper function to trim markdown code fences from the final output
static void clean_llm_output(std::string& output) {
    auto trim_whitespace = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    };

    trim_whitespace(output);
    if (output.size() > 3 && output.substr(0, 3) == "```") {
        size_t first_newline = output.find('\n');
        if (first_newline != std::string::npos) {
            output = output.substr(first_newline + 1);
        }
    }
    if (output.size() > 3 && output.substr(output.size() - 3) == "```") {
        output = output.substr(0, output.size() - 3);
    }
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

        // Create JSON request body with streaming enabled.
        nlohmann::json request_body = {
            {"model", m_model_name},
            {"prompt", prompt},
            {"stream", true} // Enable streaming for real-time output
        };

        std::cout << "[INFO] Sending request to Ollama server..." << std::endl;

        std::string accumulated_response;

        // Use a content receiver lambda to process the streaming response.
        auto res = client.Post(
            "/api/generate",
            request_body.dump(),
            "application/json",
            [&](const char *data, size_t data_length) {
                // Each chunk from Ollama is a separate JSON object on a new line.
                std::stringstream ss(std::string(data, data_length));
                std::string line;
                while (std::getline(ss, line)) {
                    if (line.empty()) continue;
                    try {
                        auto json_chunk = nlohmann::json::parse(line);
                        if (json_chunk.contains("response")) {
                            std::string chunk_text = json_chunk["response"];
                            std::cout << chunk_text << std::flush; // Print each piece as it arrives
                            accumulated_response += chunk_text;
                        }
                    } catch (const nlohmann::json::exception& e) {
                        // Ignore lines that are not valid JSON
                    }
                }
                return true; // Keep the connection open for more data
            }
        );

        std::cout << std::endl; // Final newline after streaming is done

        // Error checking after the request is complete
        if (!res) {
            auto err = res.error();
            throw std::runtime_error("Failed to connect to Ollama server: " + httplib::to_string(err));
        }

        if (res->status != 200) {
            throw std::runtime_error("Ollama server returned error status: " +
                                   std::to_string(res->status) +
                                   " - " + res->body);
        }

        // Clean the final accumulated string
        clean_llm_output(accumulated_response);
        return accumulated_response;

    } catch (const std::exception& e) {
        // Re-throw with a more specific context
        throw std::runtime_error("Error in Ollama interaction: " + std::string(e.what()));
    }
}

} // namespace Camus
