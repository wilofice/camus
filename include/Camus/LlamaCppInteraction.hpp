// =================================================================
// include/Camus/LlamaCppInteraction.hpp
// =================================================================
// Concrete implementation of LlmInteraction using llama.cpp.

#pragma once

#include "Camus/LlmInteraction.hpp"
#include <string>
#include <chrono>

// Forward declare llama.cpp structs to keep the header clean
struct llama_model;
struct llama_context;

namespace Camus {

class LlamaCppInteraction : public LlmInteraction {
public:
    /**
     * @brief Constructor loads the model and initializes the context.
     * @param model_path Full path to the GGUF model file.
     */
    explicit LlamaCppInteraction(const std::string& model_path);
    
    /**
     * @brief Constructor with model metadata configuration.
     * @param model_path Full path to the GGUF model file.
     * @param metadata Model metadata and capabilities
     */
    LlamaCppInteraction(const std::string& model_path, const ModelMetadata& metadata);
    
    ~LlamaCppInteraction() override;

    // Disable copy and move semantics for this class as it manages raw pointers.
    LlamaCppInteraction(const LlamaCppInteraction&) = delete;
    LlamaCppInteraction& operator=(const LlamaCppInteraction&) = delete;
    LlamaCppInteraction(LlamaCppInteraction&&) = delete;
    LlamaCppInteraction& operator=(LlamaCppInteraction&&) = delete;

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
    llama_model* m_model = nullptr;
    llama_context* m_context = nullptr;
    ModelMetadata m_metadata;
    mutable ModelPerformance m_performance;
    std::chrono::system_clock::time_point m_last_health_check;
    mutable bool m_is_healthy = false;
    std::string m_model_path;
    
    /**
     * @brief Initialize default metadata based on model characteristics
     */
    void initializeDefaultMetadata();
    
    /**
     * @brief Update performance metrics
     */
    void updatePerformanceMetrics(const InferenceResponse& response) const;
    
    /**
     * @brief Generate a unique model ID based on model path and characteristics
     */
    std::string generateModelId() const;
};

} // namespace Camus