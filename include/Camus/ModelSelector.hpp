// =================================================================
// include/Camus/ModelSelector.hpp
// =================================================================
// Intelligent model selector for optimal model selection based on task and context.

#pragma once

#include "Camus/ModelRegistry.hpp"
#include "Camus/TaskClassifier.hpp"
#include "Camus/ModelCapabilities.hpp"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>

namespace Camus {

/**
 * @brief Selection criteria for model selection
 */
struct SelectionCriteria {
    TaskType task_type = TaskType::UNKNOWN;      ///< Type of task to perform
    size_t context_size = 0;                      ///< Required context size in tokens
    size_t expected_output_size = 0;              ///< Expected output size in tokens
    double max_latency_ms = 0.0;                  ///< Maximum acceptable latency
    bool prefer_quality = false;                  ///< Prefer quality over speed
    bool require_code_capability = false;         ///< Require code specialization
    std::vector<std::string> required_capabilities; ///< Required model capabilities
    std::unordered_map<std::string, double> custom_weights; ///< Custom scoring weights
};

/**
 * @brief Result of model selection
 */
struct SelectionResult {
    std::string selected_model;                   ///< Selected model identifier
    double confidence_score = 0.0;                ///< Confidence in selection (0-1)
    std::string selection_reason;                 ///< Human-readable reason
    std::vector<std::pair<std::string, double>> alternatives; ///< Alternative models with scores
    std::chrono::milliseconds selection_time{0}; ///< Time taken to select
};

/**
 * @brief Model performance statistics
 */
struct ModelStatistics {
    size_t total_requests = 0;                    ///< Total requests handled
    size_t successful_requests = 0;               ///< Successful completions
    double average_latency_ms = 0.0;              ///< Average response time
    double success_rate = 0.0;                    ///< Success rate (0-1)
    double average_quality_score = 0.0;           ///< Average quality score (0-1)
    std::chrono::system_clock::time_point last_used; ///< Last usage time
    std::unordered_map<std::string, double> task_performance; ///< Performance by task type
};

/**
 * @brief Selection strategy base class
 */
class SelectionStrategy {
public:
    virtual ~SelectionStrategy() = default;
    
    /**
     * @brief Select a model based on criteria
     * @param criteria Selection criteria
     * @param available_models Available models with their configs
     * @param model_stats Current model statistics
     * @return Selection result
     */
    virtual SelectionResult selectModel(
        const SelectionCriteria& criteria,
        const std::vector<ModelConfig>& available_models,
        const std::unordered_map<std::string, ModelStatistics>& model_stats
    ) = 0;
    
    /**
     * @brief Get strategy name
     * @return Strategy identifier
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief Configuration for ModelSelector
 */
struct ModelSelectorConfig {
    std::string default_strategy = "score_based"; ///< Default selection strategy
    bool enable_learning = true;                  ///< Enable adaptive learning
    bool enable_fallback = true;                  ///< Enable fallback models
    double min_confidence_threshold = 0.3;        ///< Minimum confidence for selection
    size_t performance_history_size = 100;        ///< Number of requests to track
    std::chrono::minutes stats_retention{60};     ///< How long to retain statistics
    std::unordered_map<std::string, double> default_weights; ///< Default scoring weights
};

/**
 * @brief Intelligent model selector for optimal model selection
 */
class ModelSelector {
public:
    /**
     * @brief Constructor
     * @param registry Model registry reference
     * @param config Selector configuration
     */
    ModelSelector(ModelRegistry& registry, const ModelSelectorConfig& config = ModelSelectorConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~ModelSelector();
    
    /**
     * @brief Select best model for given criteria
     * @param criteria Selection criteria
     * @return Selection result
     */
    virtual SelectionResult selectModel(const SelectionCriteria& criteria);
    
    /**
     * @brief Register a selection strategy
     * @param name Strategy name
     * @param strategy Strategy implementation
     */
    virtual void registerStrategy(const std::string& name, std::shared_ptr<SelectionStrategy> strategy);
    
    /**
     * @brief Set active selection strategy
     * @param strategy_name Name of strategy to use
     * @return True if strategy exists and was set
     */
    virtual bool setActiveStrategy(const std::string& strategy_name);
    
    /**
     * @brief Get current active strategy name
     * @return Active strategy name
     */
    virtual std::string getActiveStrategy() const;
    
    /**
     * @brief Record model performance
     * @param model_name Model identifier
     * @param task_type Task type performed
     * @param success Whether task succeeded
     * @param latency_ms Response time in milliseconds
     * @param quality_score Optional quality score (0-1)
     */
    virtual void recordPerformance(
        const std::string& model_name,
        TaskType task_type,
        bool success,
        double latency_ms,
        double quality_score = -1.0
    );
    
    /**
     * @brief Get model statistics
     * @param model_name Model identifier
     * @return Model statistics
     */
    virtual ModelStatistics getModelStatistics(const std::string& model_name) const;
    
    /**
     * @brief Get all model statistics
     * @return Map of model statistics
     */
    virtual std::unordered_map<std::string, ModelStatistics> getAllStatistics() const;
    
    /**
     * @brief Clear performance history for a model
     * @param model_name Model identifier
     */
    virtual void clearStatistics(const std::string& model_name);
    
    /**
     * @brief Clear all performance history
     */
    virtual void clearAllStatistics();
    
    /**
     * @brief Get selector configuration
     * @return Current configuration
     */
    virtual ModelSelectorConfig getConfig() const;
    
    /**
     * @brief Update selector configuration
     * @param config New configuration
     */
    virtual void setConfig(const ModelSelectorConfig& config);
    
    /**
     * @brief Get selection audit log
     * @param limit Maximum entries to return
     * @return Recent selection decisions
     */
    virtual std::vector<SelectionResult> getSelectionHistory(size_t limit = 10) const;
    
    /**
     * @brief Analyze selection patterns
     * @return Analysis report as formatted string
     */
    virtual std::string analyzeSelectionPatterns() const;

protected:
    /**
     * @brief Calculate model score for given criteria
     * @param model Model configuration
     * @param criteria Selection criteria
     * @param stats Model statistics
     * @return Score (0-1)
     */
    virtual double calculateModelScore(
        const ModelConfig& model,
        const SelectionCriteria& criteria,
        const ModelStatistics& stats
    );
    
    /**
     * @brief Check if model meets requirements
     * @param model Model configuration
     * @param criteria Selection criteria
     * @return True if model meets all requirements
     */
    virtual bool meetsRequirements(
        const ModelConfig& model,
        const SelectionCriteria& criteria
    ) const;
    
    /**
     * @brief Update statistics with new performance data
     * @param stats Current statistics
     * @param success Whether request succeeded
     * @param latency_ms Response time
     * @param quality_score Quality score if available
     */
    virtual void updateStatistics(
        ModelStatistics& stats,
        bool success,
        double latency_ms,
        double quality_score
    );

private:
    ModelRegistry& m_registry;
    ModelSelectorConfig m_config;
    std::unordered_map<std::string, std::shared_ptr<SelectionStrategy>> m_strategies;
    std::string m_active_strategy;
    std::unordered_map<std::string, ModelStatistics> m_model_stats;
    std::vector<SelectionResult> m_selection_history;
    mutable std::mutex m_stats_mutex;
    
    class RuleBasedStrategy;
    class ScoreBasedStrategy;
    class LearningBasedStrategy;
};

} // namespace Camus