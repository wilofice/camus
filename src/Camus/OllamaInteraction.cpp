// =================================================================
// src/Camus/OllamaInteraction.cpp
// =================================================================

#include "Camus/OllamaInteraction.hpp"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <iostream>
#include <stdexcept>

namespace Camus {

OllamaInteraction::OllamaInteraction(const std::string& server_url, const std::string& model_name)
    : m_server_url(server_url), m_model_name(model_name) {
    std::cout << "[INFO] Configured Ollama client for server: " << server_url 
              << " with model: " << model_name << std::endl;
}

std::string OllamaInteraction::getCompletion(const std::string& prompt) {
    try {
        // Parse the server URL to extract host and port
        std::string protocol, host;
        int port = 80;
        
        // Simple URL parsing (assumes format like "http://localhost:11434")
        size_t protocol_end = m_server_url.find("://");
        if (protocol_end != std::string::npos) {
            protocol = m_server_url.substr(0, protocol_end);
            size_t host_start = protocol_end + 3;
            size_t port_sep = m_server_url.find(':', host_start);
            
            if (port_sep != std::string::npos) {
                host = m_server_url.substr(host_start, port_sep - host_start);
                port = std::stoi(m_server_url.substr(port_sep + 1));
            } else {
                host = m_server_url.substr(host_start);
                if (protocol == "https") {
                    port = 443;
                }
            }
        } else {
            throw std::runtime_error("Invalid server URL format: " + m_server_url);
        }
        
        // Create HTTP client
        httplib::Client client(host, port);
        client.set_connection_timeout(30);  // 30 seconds
        client.set_read_timeout(300);      // 5 minutes for long generations
        
        // Create JSON request body
        nlohmann::json request_body = {
            {"model", m_model_name},
            {"prompt", prompt},
            {"stream", false}
        };
        
        std::cout << "[INFO] Sending request to Ollama server..." << std::endl;
        
        // Make POST request
        auto response = client.Post("/api/generate", 
                                   request_body.dump(), 
                                   "application/json");
        
        // Check if request was successful
        if (!response) {
            throw std::runtime_error("Failed to connect to Ollama server at " + m_server_url);
        }
        
        if (response->status != 200) {
            throw std::runtime_error("Ollama server returned error status: " + 
                                   std::to_string(response->status) + 
                                   " - " + response->body);
        }
        
        // Parse response JSON
        auto response_json = nlohmann::json::parse(response->body);
        
        // Extract the generated text
        if (!response_json.contains("response")) {
            throw std::runtime_error("Ollama response missing 'response' field");
        }
        
        std::string generated_text = response_json["response"].get<std::string>();
        
        // Print the response as it comes (similar to LlamaCppInteraction)
        std::cout << generated_text << std::endl;
        
        return generated_text;
        
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("Error in Ollama interaction: " + std::string(e.what()));
    }
}

} // namespace Camus