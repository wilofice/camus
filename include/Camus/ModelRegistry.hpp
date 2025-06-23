// =================================================================
// include/Camus/ModelRegistry.hpp
// =================================================================
// Centralized registry for model discovery and management.

#pragma once

#include "Camus/ModelPool.hpp"
#include "Camus/ModelCapabilities.hpp"
#include "Camus/LlmInteraction.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

namespace Camus {

/**
 * @brief Model configuration from YAML
 */
struct ModelConfig {
    std::string name;                      ///< Model identifier
    std::string type;                      ///< Model type (llama_cpp, ollama, etc.)
    std::string path;                      ///< Path to model file or identifier
    std::string server_url;                ///< Server URL for remote models
    std::string model_name;                ///< Model name for remote models
    std::string description;               ///< Human-readable description
    std::string version;                   ///< Model version
    std::vector<std::string> capabilities; ///< List of capability strings
    size_t max_tokens = 4096;             ///< Maximum context tokens
    size_t max_output_tokens = 2048;      ///< Maximum output tokens
    std::string memory_usage;              ///< Expected memory usage (e.g., "8GB")
    double expected_tokens_per_second = 0.0; ///< Expected performance
    double expected_latency_ms = 0.0;      ///< Expected latency
    std::unordered_map<std::string, std::string> custom_attributes; ///< Custom config
};

/**
 * @brief Registry configuration
 */
struct RegistryConfig {
    std::string config_file_path = "config/models.yml"; ///< Path to models config
    bool auto_discover = true;             ///< Auto-discover models on startup
    bool validate_on_load = true;          ///< Validate models when loading
    bool enable_health_checks = true;      ///< Enable periodic health checks
    std::chrono::minutes health_check_interval{5}; ///< Health check interval
    bool warmup_on_load = false;           ///< Warm up models on load
    size_t max_load_retries = 3;           ///< Max retries for model loading
    std::chrono::seconds retry_delay{5};   ///< Delay between retries
};

/**
 * @brief Model loading result
 */
struct ModelLoadResult {
    bool success = false;                  ///< Whether loading succeeded
    std::string model_id;                  ///< Model identifier
    std::string error_message;             ///< Error message if failed
    std::chrono::milliseconds load_time{0}; ///< Time taken to load
};

/**
 * @brief Registry status information
 */
struct RegistryStatus {
    size_t total_configured = 0;           ///< Total models in config
    size_t successfully_loaded = 0;        ///< Successfully loaded models
    size_t failed_to_load = 0;             ///< Failed to load
    size_t currently_healthy = 0;          ///< Currently healthy models
    std::chrono::system_clock::time_point last_update; ///< Last status update
    std::vector<ModelLoadResult> load_results; ///< Individual load results
};

/**
 * @brief Model factory function type
 */
using ModelFactory = std::function<std::shared_ptr<LlmInteraction>(const ModelConfig&)>;

/**
 * @brief Centralized model registry for discovery and management
 */
class ModelRegistry {
public:
    /**
     * @brief Constructor with optional configuration
     */
    explicit ModelRegistry(const RegistryConfig& config = RegistryConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~ModelRegistry();

    /**
     * @brief Load models from configuration file
     * @param config_path Path to YAML configuration file
     * @return Registry status after loading
     */
    virtual RegistryStatus loadFromConfig(const std::string& config_path);
    
    /**
     * @brief Reload configuration and update models
     * @return Registry status after reloading
     */
    virtual RegistryStatus reloadConfiguration();
    
    /**
     * @brief Register a model factory for a specific type
     * @param model_type The model type identifier
     * @param factory The factory function to create models
     */
    virtual void registerModelFactory(const std::string& model_type, ModelFactory factory);
    
    /**
     * @brief Get a model by name
     * @param model_name The model identifier
     * @return Shared pointer to model, or nullptr if not found
     */
    virtual std::shared_ptr<LlmInteraction> getModel(const std::string& model_name);
    
    /**
     * @brief Get the model pool
     * @return Reference to the internal model pool
     */
    virtual ModelPool& getModelPool();
    
    /**
     * @brief Get registry status
     * @return Current registry status
     */
    virtual RegistryStatus getStatus() const;
    
    /**
     * @brief Get all configured models
     * @return Vector of model configurations
     */
    virtual std::vector<ModelConfig> getConfiguredModels() const;
    
    /**
     * @brief Get all loaded models
     * @return Vector of loaded model names
     */
    virtual std::vector<std::string> getLoadedModels() const;
    
    /**
     * @brief Perform health checks on all models
     * @return Number of healthy models
     */
    virtual size_t performHealthChecks();
    
    /**
     * @brief Validate a model configuration
     * @param config The model configuration to validate
     * @return True if configuration is valid
     */
    virtual bool validateModelConfig(const ModelConfig& config) const;
    
    /**
     * @brief Load a single model from configuration
     * @param config The model configuration
     * @return Load result
     */
    virtual ModelLoadResult loadModel(const ModelConfig& config);
    
    /**
     * @brief Unload a model
     * @param model_name The model identifier
     * @return True if successfully unloaded
     */
    virtual bool unloadModel(const std::string& model_name);
    
    /**
     * @brief Test a model with a simple prompt
     * @param model_name The model identifier
     * @return Test result and response time
     */
    virtual std::pair<bool, std::chrono::milliseconds> testModel(const std::string& model_name);
    
    /**
     * @brief Get model info as formatted string (for CLI)
     * @param model_name The model identifier
     * @return Formatted model information
     */
    virtual std::string getModelInfo(const std::string& model_name) const;
    
    /**
     * @brief Get all models info as formatted string (for CLI)
     * @return Formatted information for all models
     */
    virtual std::string getAllModelsInfo() const;
    
    /**
     * @brief Set the registry configuration
     * @param config New configuration
     */
    virtual void setConfig(const RegistryConfig& config);
    
    /**
     * @brief Get the registry configuration
     * @return Current configuration
     */
    virtual RegistryConfig getConfig() const;

protected:
    /**
     * @brief Parse YAML configuration file
     * @param config_path Path to configuration file
     * @return Vector of model configurations
     */
    virtual std::vector<ModelConfig> parseConfigFile(const std::string& config_path);
    
    /**
     * @brief Create model metadata from configuration
     * @param config Model configuration
     * @return Model metadata
     */
    virtual ModelMetadata createMetadata(const ModelConfig& config);
    
    /**
     * @brief Default factory for llama_cpp models
     */
    std::shared_ptr<LlmInteraction> createLlamaCppModel(const ModelConfig& config);
    
    /**
     * @brief Default factory for ollama models
     */
    std::shared_ptr<LlmInteraction> createOllamaModel(const ModelConfig& config);
    
    /**
     * @brief Start background health check thread
     */
    void startHealthCheckThread();
    
    /**
     * @brief Stop background health check thread
     */
    void stopHealthCheckThread();

private:
    RegistryConfig m_config;
    std::unique_ptr<ConcreteModelPool> m_model_pool;
    std::unordered_map<std::string, ModelFactory> m_factories;
    std::unordered_map<std::string, ModelConfig> m_model_configs;
    RegistryStatus m_status;
    
    // Health check thread management
    std::unique_ptr<std::thread> m_health_check_thread;
    std::atomic<bool> m_stop_health_checks{false};
    mutable std::mutex m_registry_mutex;
    
    /**
     * @brief Health check thread function
     */
    void healthCheckLoop();
    
    /**
     * @brief Update registry status
     */
    void updateStatus();
    
    /**
     * @brief Parse memory string (e.g., "8GB") to bytes
     */
    size_t parseMemoryString(const std::string& memory_str) const;
};

} // namespace Camus