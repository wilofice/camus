// =================================================================
// include/Camus/ModelCapabilities.hpp
// =================================================================
// Defines model capabilities and metadata system for the multi-model pipeline.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace Camus {

/**
 * @brief Enumeration of model capability flags
 */
enum class ModelCapability {
    FAST_INFERENCE,     ///< Optimized for speed over quality
    HIGH_QUALITY,       ///< Best output quality, may be slower
    CODE_SPECIALIZED,   ///< Trained specifically on code tasks
    LARGE_CONTEXT,      ///< Supports extended context windows (>8k tokens)
    SECURITY_FOCUSED,   ///< Enhanced security validation and analysis
    MULTILINGUAL,       ///< Strong support for multiple languages
    REASONING,          ///< Strong logical reasoning capabilities
    CREATIVE_WRITING,   ///< Optimized for creative text generation
    INSTRUCTION_TUNED,  ///< Trained to follow complex instructions
    CHAT_OPTIMIZED     ///< Designed for conversational interactions
};

/**
 * @brief Model performance characteristics
 */
struct ModelPerformance {
    double tokens_per_second = 0.0;        ///< Average tokens generated per second
    std::chrono::milliseconds avg_latency{0}; ///< Average response latency
    double memory_usage_gb = 0.0;          ///< Memory usage in GB
    double cpu_usage_percent = 0.0;        ///< CPU usage percentage
    double gpu_usage_percent = 0.0;        ///< GPU usage percentage (if applicable)
    size_t max_context_tokens = 4096;      ///< Maximum context window size
    size_t max_output_tokens = 2048;       ///< Maximum output tokens per response
};

/**
 * @brief Comprehensive model metadata
 */
struct ModelMetadata {
    std::string name;                       ///< Model name/identifier
    std::string description;                ///< Human-readable description
    std::string version;                    ///< Model version
    std::string provider;                   ///< Model provider (e.g., "llama_cpp", "ollama")
    std::string model_path;                 ///< Path to model file or identifier
    std::vector<ModelCapability> capabilities; ///< List of model capabilities
    ModelPerformance performance;           ///< Performance characteristics
    std::unordered_map<std::string, std::string> custom_attributes; ///< Custom metadata
    
    // Health and availability
    bool is_available = false;              ///< Whether model is currently available
    bool is_healthy = false;                ///< Whether model is functioning properly
    std::chrono::system_clock::time_point last_health_check; ///< Last health check time
    std::string health_status_message;      ///< Human-readable health status
};

/**
 * @brief Utility functions for model capabilities
 */
class ModelCapabilityUtils {
public:
    /**
     * @brief Convert capability enum to string representation
     */
    static std::string capabilityToString(ModelCapability capability);
    
    /**
     * @brief Convert string to capability enum
     * @return ModelCapability or throws if invalid
     */
    static ModelCapability stringToCapability(const std::string& str);
    
    /**
     * @brief Get all available capabilities
     */
    static std::vector<ModelCapability> getAllCapabilities();
    
    /**
     * @brief Check if a model has a specific capability
     */
    static bool hasCapability(const ModelMetadata& metadata, ModelCapability capability);
    
    /**
     * @brief Get capabilities as string list for logging/display
     */
    static std::vector<std::string> capabilitiesToStrings(const std::vector<ModelCapability>& capabilities);
    
    /**
     * @brief Parse capabilities from string list
     */
    static std::vector<ModelCapability> parseCapabilities(const std::vector<std::string>& capability_strings);
};

} // namespace Camus