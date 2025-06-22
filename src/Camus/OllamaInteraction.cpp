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

        // Create JSON request body without streaming for now
        nlohmann::json request_body = {
            {"model", m_model_name},
            {"prompt", prompt},
            {"stream", false} // Disable streaming for simpler implementation
        };

        std::cout << "[INFO] Sending request to Ollama server..." << std::endl;

        // Make POST request
        auto res = client.Post("/api/generate",
                              request_body.dump(),
                              "application/json");

        // Error checking
        if (!res) {
            throw std::runtime_error("Failed to connect to Ollama server at " + m_server_url);
        }

        if (res->status != 200) {
            throw std::runtime_error("Ollama server returned error status: " +
                                   std::to_string(res->status) +
                                   " - " + res->body);
        }

        // Parse response JSON
        auto response_json = nlohmann::json::parse(res->body);
        
        // Extract the generated text
        if (!response_json.contains("response")) {
            throw std::runtime_error("Ollama response missing 'response' field");
        }
        
        std::string generated_text = response_json["response"].get<std::string>();
        
        // Print the response
        std::cout << generated_text << std::endl;
        
        // Clean the output
        clean_llm_output(generated_text);
        return generated_text;

    } catch (const std::exception& e) {
        // Re-throw with a more specific context
        throw std::runtime_error("Error in Ollama interaction: " + std::string(e.what()));
    }
}

} // namespace Camus
