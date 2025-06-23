// =================================================================
// include/Camus/OllamaInteraction.hpp (New File)
// =================================================================
// Defines the interface for interacting with the Ollama server.

#pragma once

#include "Camus/LlmInteraction.hpp"
#include <chrono>

namespace Camus {

class OllamaInteraction : public LlmInteraction {
public:
    /**
     * @brief Constructs the Ollama client.
     * @param server_url The base URL of the Ollama server (e.g., http://localhost:11434).
     * @param model_name The name of the model to use (e.g., llama3:latest).
     */
    OllamaInteraction(const std::string& server_url, const std::string& model_name);
    
    /**
     * @brief Constructor with model metadata configuration.
     * @param server_url The base URL of the Ollama server
     * @param model_name The name of the model to use
     * @param metadata Model metadata and capabilities
     */
    OllamaInteraction(const std::string& server_url, const std::string& model_name, const ModelMetadata& metadata);

    // LlmInteraction interface implementation
    std::string getCompletion(const std::string& prompt) override;
    InferenceResponse getCompletionWithMetadata(const InferenceRequest& request) override;
    ModelMetadata getModelMetadata() const override;
    bool isHealthy() const override;
    bool performHealthCheck() override;
    ModelPerformance getCurrentPerformance() const override;
    bool warmUp() override;
    void cleanup() override;
    std::string getModelId() const override;

private:
    std::string m_server_url;
    std::string m_model_name;
    ModelMetadata m_metadata;
    mutable ModelPerformance m_performance;
    std::chrono::system_clock::time_point m_last_health_check;
    mutable bool m_is_healthy = false;
    
    /**
     * @brief Initialize default metadata based on model characteristics
     */
    void initializeDefaultMetadata();
    
    /**
     * @brief Update performance metrics
     */
    void updatePerformanceMetrics(const InferenceResponse& response) const;
    
    /**
     * @brief Generate a unique model ID for Ollama model
     */
    std::string generateModelId() const;
    
    /**
     * @brief Make HTTP request to Ollama server
     */
    std::string makeHttpRequest(const std::string& endpoint, const std::string& payload) const;
};

} // namespace Camus
