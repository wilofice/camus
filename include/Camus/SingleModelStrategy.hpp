// =================================================================
// include/Camus/SingleModelStrategy.hpp
// =================================================================
// Enhanced single-model execution strategy with intelligent optimization.

#pragma once

#include "Camus/ModelRegistry.hpp"
#include "Camus/TaskClassifier.hpp"
#include "Camus/ModelCapabilities.hpp"
#include "Camus/LlmInteraction.hpp"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>

namespace Camus {

/**
 * @brief Strategy request structure for single model processing
 */
struct StrategyRequest {
    std::string request_id;                       ///< Unique request identifier
    std::string prompt;                           ///< User prompt
    std::string context;                          ///< Additional context
    TaskType task_type = TaskType::UNKNOWN;       ///< Classified task type
    std::string selected_model;                   ///< Target model name
    int max_tokens = 2048;                        ///< Maximum output tokens
    double temperature = 0.7;                     ///< Sampling temperature
    double top_p = 0.9;                           ///< Top-p sampling
    int top_k = 40;                               ///< Top-k sampling
    std::chrono::milliseconds timeout{30000};    ///< Request timeout
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
    bool enable_optimization = true;              ///< Enable prompt optimization
    bool enable_validation = true;                ///< Enable response validation
};

/**
 * @brief Strategy response structure
 */
struct StrategyResponse {
    std::string request_id;                       ///< Original request ID
    std::string response_text;                    ///< Generated response
    bool success = false;                         ///< Whether execution succeeded
    std::string error_message;                    ///< Error message if failed
    std::string model_used;                       ///< Model that generated response
    std::string optimized_prompt;                 ///< Optimized prompt used
    double quality_score = 0.0;                   ///< Response quality score (0-1)
    std::chrono::milliseconds execution_time{0}; ///< Model execution time
    std::chrono::milliseconds optimization_time{0}; ///< Prompt optimization time
    std::chrono::milliseconds validation_time{0}; ///< Response validation time
    std::chrono::milliseconds total_time{0};     ///< Total processing time
    size_t tokens_generated = 0;                  ///< Number of tokens generated
    size_t context_tokens_used = 0;               ///< Context tokens consumed
    double context_utilization = 0.0;            ///< Context window utilization (0-1)
    std::unordered_map<std::string, std::string> debug_info; ///< Debug information
    std::vector<std::string> optimization_steps; ///< Optimization steps applied
};

/**
 * @brief Prompt template for model-specific optimization
 */
struct PromptTemplate {
    std::string template_name;                    ///< Template identifier
    std::string system_prompt;                    ///< System/instruction prompt
    std::string user_prompt_format;               ///< User prompt format string
    std::string context_format;                   ///< Context integration format
    std::unordered_map<TaskType, std::string> task_specific_instructions; ///< Task-specific prompts
    std::vector<ModelCapability> target_capabilities; ///< Target model capabilities
    double temperature_adjustment = 0.0;         ///< Temperature adjustment
    int max_tokens_adjustment = 0;                ///< Max tokens adjustment
};

/**
 * @brief Context window management configuration
 */
struct ContextWindowConfig {
    size_t max_context_tokens = 4096;            ///< Maximum context window size
    size_t reserved_output_tokens = 512;         ///< Tokens reserved for output
    double utilization_target = 0.85;            ///< Target utilization (0-1)
    bool enable_truncation = true;                ///< Enable intelligent truncation
    bool enable_summarization = false;           ///< Enable context summarization
    std::string truncation_strategy = "tail";    ///< "head", "tail", "middle", "smart"
};

/**
 * @brief Model-specific parameter tuning configuration
 */
struct ModelParameterConfig {
    std::unordered_map<ModelCapability, double> temperature_adjustments; ///< Temperature by capability
    std::unordered_map<ModelCapability, double> top_p_adjustments;       ///< Top-p by capability
    std::unordered_map<ModelCapability, int> top_k_adjustments;          ///< Top-k by capability
    std::unordered_map<TaskType, double> task_temperature_adjustments;   ///< Temperature by task
    std::unordered_map<TaskType, int> task_max_tokens_adjustments;       ///< Max tokens by task
    bool enable_adaptive_parameters = true;      ///< Enable parameter adaptation
    double adaptation_learning_rate = 0.1;       ///< Learning rate for adaptation
};

/**
 * @brief Configuration for SingleModelStrategy
 */
struct SingleModelStrategyConfig {
    bool enable_prompt_optimization = true;      ///< Enable intelligent prompt optimization
    bool enable_context_management = true;       ///< Enable dynamic context management
    bool enable_parameter_tuning = true;         ///< Enable model-specific parameter tuning
    bool enable_response_validation = true;      ///< Enable response quality validation
    bool enable_performance_monitoring = true;   ///< Enable performance tracking
    
    ContextWindowConfig context_config;          ///< Context window management settings
    ModelParameterConfig parameter_config;       ///< Parameter tuning settings
    
    std::unordered_map<std::string, PromptTemplate> model_templates; ///< Model-specific templates
    std::unordered_map<TaskType, PromptTemplate> task_templates;     ///< Task-specific templates
    
    double min_quality_threshold = 0.3;          ///< Minimum acceptable quality
    size_t max_optimization_attempts = 3;        ///< Maximum optimization retries
    std::chrono::milliseconds optimization_timeout{5000}; ///< Optimization timeout
};

/**
 * @brief Performance statistics for single model strategy
 */
struct SingleModelStatistics {
    size_t total_requests = 0;                   ///< Total requests processed
    size_t successful_requests = 0;              ///< Successful requests
    size_t failed_requests = 0;                  ///< Failed requests
    size_t optimized_requests = 0;               ///< Requests with optimization applied
    double average_execution_time = 0.0;         ///< Average execution time in ms
    double average_optimization_time = 0.0;      ///< Average optimization time in ms
    double average_quality_score = 0.0;          ///< Average quality score
    double average_context_utilization = 0.0;    ///< Average context utilization
    std::unordered_map<std::string, size_t> model_usage; ///< Usage count by model
    std::unordered_map<TaskType, size_t> task_distribution; ///< Request distribution by task
    std::unordered_map<std::string, double> optimization_effectiveness; ///< Optimization impact
    std::chrono::system_clock::time_point last_reset; ///< Last statistics reset
};

/**
 * @brief Enhanced single-model execution strategy
 * 
 * Provides intelligent prompt optimization, dynamic context management,
 * model-specific parameter tuning, and response validation for optimal
 * single-model performance.
 */
class SingleModelStrategy {
public:
    /**
     * @brief Constructor
     * @param registry Model registry reference
     * @param config Strategy configuration
     */
    SingleModelStrategy(ModelRegistry& registry, 
                       const SingleModelStrategyConfig& config = SingleModelStrategyConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~SingleModelStrategy() = default;
    
    /**
     * @brief Execute request using single model with optimizations
     * @param request Strategy request
     * @return Strategy response
     */
    virtual StrategyResponse execute(const StrategyRequest& request);
    
    /**
     * @brief Get strategy configuration
     * @return Current configuration
     */
    virtual SingleModelStrategyConfig getConfig() const;
    
    /**
     * @brief Set strategy configuration
     * @param config New configuration
     */
    virtual void setConfig(const SingleModelStrategyConfig& config);
    
    /**
     * @brief Get strategy statistics
     * @return Current statistics
     */
    virtual SingleModelStatistics getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    virtual void resetStatistics();
    
    /**
     * @brief Register custom prompt template
     * @param model_name Model name or capability
     * @param template_obj Prompt template
     */
    virtual void registerPromptTemplate(const std::string& model_name, 
                                       const PromptTemplate& template_obj);
    
    /**
     * @brief Register quality scorer function
     * @param scorer Function to score response quality (0-1)
     */
    virtual void registerQualityScorer(std::function<double(const std::string&, const std::string&)> scorer);
    
    /**
     * @brief Optimize parameters based on performance feedback
     * @param model_name Model name
     * @param task_type Task type
     * @param quality_score Achieved quality score
     * @param execution_time Execution time
     */
    virtual void adaptParameters(const std::string& model_name, 
                                TaskType task_type,
                                double quality_score, 
                                std::chrono::milliseconds execution_time);

protected:
    /**
     * @brief Optimize prompt for specific model and task
     * @param request Strategy request
     * @param model_metadata Model metadata
     * @return Optimized prompt
     */
    virtual std::string optimizePrompt(const StrategyRequest& request, 
                                      const ModelMetadata& model_metadata);
    
    /**
     * @brief Manage context window and truncation
     * @param prompt Original prompt
     * @param context Additional context
     * @param model_metadata Model metadata
     * @return Managed context string
     */
    virtual std::string manageContextWindow(const std::string& prompt,
                                           const std::string& context,
                                           const ModelMetadata& model_metadata);
    
    /**
     * @brief Tune model parameters for optimal performance
     * @param request Strategy request
     * @param model_metadata Model metadata
     * @return Tuned request parameters
     */
    virtual StrategyRequest tuneParameters(const StrategyRequest& request,
                                          const ModelMetadata& model_metadata);
    
    /**
     * @brief Validate and score response quality
     * @param request Original request
     * @param response Generated response
     * @param model_metadata Model metadata
     * @return Quality score (0-1)
     */
    virtual double validateResponse(const StrategyRequest& request,
                                   const std::string& response,
                                   const ModelMetadata& model_metadata);
    
    /**
     * @brief Apply model-specific optimizations
     * @param prompt Original prompt
     * @param model_metadata Model metadata
     * @param task_type Task type
     * @return Optimized prompt
     */
    virtual std::string applyModelSpecificOptimizations(const std::string& prompt,
                                                        const ModelMetadata& model_metadata,
                                                        TaskType task_type);
    
    /**
     * @brief Truncate context intelligently based on strategy
     * @param text Text to truncate
     * @param max_tokens Maximum tokens allowed
     * @param strategy Truncation strategy
     * @return Truncated text
     */
    virtual std::string truncateContext(const std::string& text,
                                       size_t max_tokens,
                                       const std::string& strategy);
    
    /**
     * @brief Estimate token count for text
     * @param text Input text
     * @return Estimated token count
     */
    virtual size_t estimateTokenCount(const std::string& text);
    
    /**
     * @brief Update performance statistics
     * @param request Original request
     * @param response Strategy response
     */
    virtual void updateStatistics(const StrategyRequest& request, 
                                 const StrategyResponse& response);

private:
    ModelRegistry& m_registry;
    SingleModelStrategyConfig m_config;
    SingleModelStatistics m_statistics;
    
    std::unordered_map<std::string, PromptTemplate> m_model_templates;
    std::unordered_map<TaskType, PromptTemplate> m_task_templates;
    std::function<double(const std::string&, const std::string&)> m_quality_scorer;
    
    mutable std::mutex m_stats_mutex;
    mutable std::mutex m_config_mutex;
    
    /**
     * @brief Initialize default prompt templates
     */
    void initializeDefaultTemplates();
    
    /**
     * @brief Initialize default parameter configurations
     */
    void initializeDefaultParameters();
    
    /**
     * @brief Get prompt template for model and task
     * @param model_metadata Model metadata
     * @param task_type Task type
     * @return Prompt template
     */
    PromptTemplate getPromptTemplate(const ModelMetadata& model_metadata, 
                                    TaskType task_type);
    
    /**
     * @brief Apply prompt template formatting
     * @param template_obj Prompt template
     * @param prompt Original prompt
     * @param context Additional context
     * @return Formatted prompt
     */
    std::string applyPromptTemplate(const PromptTemplate& template_obj,
                                   const std::string& prompt,
                                   const std::string& context);
    
    /**
     * @brief Default quality scoring function
     * @param prompt Original prompt
     * @param response Generated response
     * @return Quality score (0-1)
     */
    double defaultQualityScorer(const std::string& prompt, const std::string& response);
    
    /**
     * @brief Extract keywords for optimization
     * @param text Input text
     * @return Vector of keywords
     */
    std::vector<std::string> extractKeywords(const std::string& text);
    
    /**
     * @brief Check if model has specific capability
     * @param model_metadata Model metadata
     * @param capability Capability to check
     * @return True if model has capability
     */
    bool hasCapability(const ModelMetadata& model_metadata, ModelCapability capability);
};

} // namespace Camus