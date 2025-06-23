// =================================================================
// include/Camus/ModelPool.hpp
// =================================================================
// Abstract model pool for managing multiple LLM instances.

#pragma once

#include "Camus/LlmInteraction.hpp"
#include "Camus/ModelCapabilities.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace Camus {

/**
 * @brief Model selection criteria for choosing the best model for a task
 */
struct ModelSelectionCriteria {
    std::vector<ModelCapability> required_capabilities; ///< Must have these capabilities
    std::vector<ModelCapability> preferred_capabilities; ///< Nice to have these capabilities
    size_t min_context_tokens = 0;         ///< Minimum context window size needed
    size_t max_context_tokens = SIZE_MAX;  ///< Maximum context window size (for efficiency)
    double max_latency_ms = 30000.0;       ///< Maximum acceptable latency in milliseconds
    double min_quality_score = 0.0;        ///< Minimum quality score (0.0-1.0)
    bool prefer_speed = false;              ///< Prefer faster models over higher quality
    bool require_availability = true;      ///< Only consider available/healthy models
};

/**
 * @brief Model pool statistics
 */
struct ModelPoolStats {
    size_t total_models = 0;               ///< Total number of models in pool
    size_t available_models = 0;           ///< Number of available models
    size_t healthy_models = 0;             ///< Number of healthy models
    size_t total_requests = 0;             ///< Total requests processed
    size_t failed_requests = 0;            ///< Total failed requests
    std::chrono::system_clock::time_point last_update; ///< Last stats update time
    std::unordered_map<std::string, size_t> model_usage_counts; ///< Usage per model
};

/**
 * @brief Abstract base class for managing a pool of LLM models
 */
class ModelPool {
public:
    virtual ~ModelPool() = default;

    /**
     * @brief Add a model instance to the pool
     * @param model Shared pointer to the model instance
     * @return True if model was successfully added
     */
    virtual bool addModel(std::shared_ptr<LlmInteraction> model) = 0;
    
    /**
     * @brief Remove a model from the pool by ID
     * @param model_id The unique identifier of the model to remove
     * @return True if model was found and removed
     */
    virtual bool removeModel(const std::string& model_id) = 0;
    
    /**
     * @brief Get a specific model by ID
     * @param model_id The unique identifier of the model
     * @return Shared pointer to the model, or nullptr if not found
     */
    virtual std::shared_ptr<LlmInteraction> getModel(const std::string& model_id) = 0;
    
    /**
     * @brief Select the best model based on criteria
     * @param criteria The selection criteria to use
     * @return Shared pointer to the selected model, or nullptr if none match
     */
    virtual std::shared_ptr<LlmInteraction> selectModel(const ModelSelectionCriteria& criteria) = 0;
    
    /**
     * @brief Get all models matching the given criteria
     * @param criteria The selection criteria to use
     * @return Vector of models that match the criteria
     */
    virtual std::vector<std::shared_ptr<LlmInteraction>> getModelsMatching(const ModelSelectionCriteria& criteria) = 0;
    
    /**
     * @brief Get all model IDs in the pool
     * @return Vector of model IDs
     */
    virtual std::vector<std::string> getAllModelIds() const = 0;
    
    /**
     * @brief Get metadata for all models in the pool
     * @return Vector of model metadata
     */
    virtual std::vector<ModelMetadata> getAllModelMetadata() const = 0;
    
    /**
     * @brief Perform health checks on all models
     * @return Number of healthy models after check
     */
    virtual size_t performHealthChecks() = 0;
    
    /**
     * @brief Get pool statistics
     * @return Current pool statistics
     */
    virtual ModelPoolStats getPoolStats() const = 0;
    
    /**
     * @brief Warm up all models in the pool
     * @return Number of models successfully warmed up
     */
    virtual size_t warmUpAll() = 0;
    
    /**
     * @brief Clean up all models and release resources
     */
    virtual void cleanupAll() = 0;
    
    /**
     * @brief Check if the pool is empty
     * @return True if no models are in the pool
     */
    virtual bool isEmpty() const = 0;
    
    /**
     * @brief Get the number of models in the pool
     * @return Total number of models
     */
    virtual size_t size() const = 0;
};

/**
 * @brief Concrete implementation of ModelPool
 */
class ConcreteModelPool : public ModelPool {
public:
    ConcreteModelPool();
    ~ConcreteModelPool() override;

    // ModelPool interface implementation
    bool addModel(std::shared_ptr<LlmInteraction> model) override;
    bool removeModel(const std::string& model_id) override;
    std::shared_ptr<LlmInteraction> getModel(const std::string& model_id) override;
    std::shared_ptr<LlmInteraction> selectModel(const ModelSelectionCriteria& criteria) override;
    std::vector<std::shared_ptr<LlmInteraction>> getModelsMatching(const ModelSelectionCriteria& criteria) override;
    std::vector<std::string> getAllModelIds() const override;
    std::vector<ModelMetadata> getAllModelMetadata() const override;
    size_t performHealthChecks() override;
    ModelPoolStats getPoolStats() const override;
    size_t warmUpAll() override;
    void cleanupAll() override;
    bool isEmpty() const override;
    size_t size() const override;

    /**
     * @brief Set custom model selection function
     * @param selector Custom selection function
     */
    void setCustomSelector(std::function<std::shared_ptr<LlmInteraction>(
        const std::vector<std::shared_ptr<LlmInteraction>>&, 
        const ModelSelectionCriteria&)> selector);

private:
    std::unordered_map<std::string, std::shared_ptr<LlmInteraction>> m_models;
    ModelPoolStats m_stats;
    std::function<std::shared_ptr<LlmInteraction>(
        const std::vector<std::shared_ptr<LlmInteraction>>&,
        const ModelSelectionCriteria&)> m_custom_selector;

    /**
     * @brief Default model selection algorithm
     */
    std::shared_ptr<LlmInteraction> defaultModelSelection(
        const std::vector<std::shared_ptr<LlmInteraction>>& candidates,
        const ModelSelectionCriteria& criteria);
        
    /**
     * @brief Calculate model score based on criteria
     */
    double calculateModelScore(
        const std::shared_ptr<LlmInteraction>& model,
        const ModelSelectionCriteria& criteria);
        
    /**
     * @brief Update pool statistics
     */
    void updateStats();
};

} // namespace Camus