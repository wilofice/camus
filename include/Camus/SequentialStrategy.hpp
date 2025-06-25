// =================================================================
// include/Camus/SequentialStrategy.hpp
// =================================================================
// Sequential strategy for chained model processing with backward compatibility.

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
 * @brief Sequential processing pattern type
 */
enum class SequentialPattern {
    ORIGINAL_REFINEMENT,    ///< Original progressive refinement (output N -> input N+1)
    ANALYSIS_GENERATION,    ///< Analyze code, then generate improvements
    GENERATION_VALIDATION,  ///< Generate code, then validate safety/quality
    PLANNING_IMPLEMENTATION, ///< Create plan, then implement changes
    REVIEW_REFINEMENT,      ///< Initial implementation, then iterative improvement
    CUSTOM                  ///< Custom user-defined pattern
};

/**
 * @brief Pipeline step definition
 */
struct PipelineStep {
    std::string step_id;                          ///< Unique step identifier
    std::string model_name;                       ///< Model to use for this step
    std::string step_description;                 ///< Human-readable description
    std::string prompt_template;                  ///< Template for step prompt
    std::unordered_map<std::string, std::string> step_parameters; ///< Step-specific parameters
    bool is_required = true;                      ///< Whether step is required
    bool cache_result = false;                    ///< Whether to cache step result
    std::chrono::milliseconds timeout{30000};    ///< Step-specific timeout
    double min_quality_threshold = 0.0;          ///< Minimum quality to proceed
    std::vector<std::string> validation_rules;   ///< Validation rules for step output
    std::string fallback_model;                  ///< Fallback model if primary fails
};

/**
 * @brief Pipeline state for passing data between steps
 */
struct PipelineState {
    std::string request_id;                       ///< Original request ID
    std::string original_input;                   ///< Original user input
    std::string current_output;                   ///< Current pipeline output
    std::unordered_map<std::string, std::string> step_outputs; ///< Output from each step
    std::unordered_map<std::string, std::string> context_variables; ///< Context variables
    std::vector<std::string> processing_history;  ///< History of transformations
    size_t current_step_index = 0;               ///< Current step in pipeline
    bool has_errors = false;                      ///< Whether errors occurred
    std::vector<std::string> error_messages;     ///< Accumulated error messages
    std::chrono::system_clock::time_point start_time; ///< Pipeline start time
    std::unordered_map<std::string, double> quality_scores; ///< Quality scores per step
};

/**
 * @brief Sequential request structure
 */
struct SequentialRequest {
    std::string request_id;                       ///< Unique request identifier
    std::string prompt;                           ///< User prompt
    std::string context;                          ///< Additional context
    std::string original_code;                    ///< Original code (for compatibility)
    TaskType task_type = TaskType::UNKNOWN;       ///< Classified task type
    SequentialPattern pattern = SequentialPattern::ORIGINAL_REFINEMENT; ///< Processing pattern
    
    // Original pipeline format support
    std::vector<std::string> backend_names;      ///< Original backend list format
    std::string pipeline_name;                   ///< Named pipeline from config
    
    // Enhanced pipeline format support
    std::vector<PipelineStep> custom_steps;      ///< Custom pipeline steps
    
    std::chrono::milliseconds timeout{120000};   ///< Total pipeline timeout
    bool enable_caching = true;                   ///< Enable intermediate caching
    bool enable_rollback = true;                  ///< Enable rollback on failure
    bool enable_validation = true;                ///< Enable step validation
    bool original_format_mode = false;           ///< Use original pipeline format
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
};

/**
 * @brief Sequential response structure
 */
struct SequentialResponse {
    std::string request_id;                       ///< Original request ID
    std::string final_output;                     ///< Final pipeline output
    bool success = false;                         ///< Whether pipeline succeeded
    std::string error_message;                    ///< Error message if failed
    SequentialPattern pattern_used;               ///< Pattern that was used
    std::vector<PipelineStep> steps_executed;    ///< Steps that were executed
    PipelineState final_state;                   ///< Final pipeline state
    std::chrono::milliseconds total_time{0};     ///< Total processing time
    std::chrono::milliseconds coordination_time{0}; ///< Time for coordination overhead
    size_t steps_completed = 0;                  ///< Number of steps completed
    size_t total_steps = 0;                      ///< Total number of steps
    std::vector<std::string> transformation_history; ///< History of transformations
    std::unordered_map<std::string, double> step_quality_scores; ///< Quality scores per step
    double overall_quality_score = 0.0;          ///< Overall pipeline quality
    std::unordered_map<std::string, std::chrono::milliseconds> step_timings; ///< Timing per step
    std::vector<std::string> pipeline_log;       ///< Console output log for compatibility
    std::unordered_map<std::string, std::string> debug_info; ///< Debug information
    bool rollback_occurred = false;               ///< Whether rollback was triggered
    std::string rollback_reason;                 ///< Reason for rollback if occurred
};

/**
 * @brief Configuration for SequentialStrategy
 */
struct SequentialStrategyConfig {
    bool enable_original_compatibility = true;   ///< Enable original pipeline format
    bool enable_enhanced_features = true;        ///< Enable enhanced pipeline features
    bool enable_step_validation = true;          ///< Enable step-by-step validation
    bool enable_intermediate_caching = true;     ///< Enable caching between steps
    bool enable_rollback = true;                 ///< Enable rollback on failure
    bool enable_console_output = true;           ///< Enable original console output format
    
    std::chrono::milliseconds default_step_timeout{30000}; ///< Default timeout per step
    std::chrono::milliseconds max_pipeline_time{300000};   ///< Maximum total pipeline time
    size_t max_rollback_attempts = 3;            ///< Maximum rollback attempts
    double min_step_quality = 0.3;               ///< Minimum quality to proceed
    double min_overall_quality = 0.5;            ///< Minimum overall quality
    
    std::unordered_map<SequentialPattern, std::vector<PipelineStep>> pattern_templates; ///< Pattern templates
    std::unordered_map<std::string, std::string> prompt_templates; ///< Prompt templates
    std::unordered_map<std::string, std::vector<std::string>> named_pipelines; ///< Named pipeline configs
    
    bool enable_performance_monitoring = true;   ///< Enable performance tracking
    bool enable_quality_tracking = true;         ///< Enable quality score tracking
    size_t max_cache_size = 100;                 ///< Maximum cached results
    std::chrono::seconds cache_ttl{3600};        ///< Cache time-to-live
};

/**
 * @brief Performance statistics for sequential strategy
 */
struct SequentialStatistics {
    size_t total_pipelines = 0;                  ///< Total pipelines executed
    size_t successful_pipelines = 0;             ///< Successful pipelines
    size_t failed_pipelines = 0;                 ///< Failed pipelines
    size_t rollback_count = 0;                   ///< Number of rollbacks
    double average_pipeline_time = 0.0;          ///< Average pipeline execution time
    double average_coordination_time = 0.0;      ///< Average coordination overhead
    double average_quality_score = 0.0;          ///< Average overall quality
    double average_steps_per_pipeline = 0.0;     ///< Average steps per pipeline
    std::unordered_map<SequentialPattern, size_t> pattern_usage; ///< Usage by pattern
    std::unordered_map<std::string, size_t> model_step_usage; ///< Model usage in steps
    std::unordered_map<std::string, double> step_success_rates; ///< Success rate per step type
    std::unordered_map<TaskType, double> task_quality_scores; ///< Quality by task type
    std::chrono::system_clock::time_point last_reset; ///< Last statistics reset
};

/**
 * @brief Cached pipeline result
 */
struct CachedResult {
    std::string input_hash;                       ///< Hash of input for cache key
    std::string cached_output;                    ///< Cached pipeline output
    PipelineState cached_state;                   ///< Cached pipeline state
    std::chrono::system_clock::time_point created_at; ///< Cache creation time
    std::chrono::system_clock::time_point last_used; ///< Last access time
    size_t access_count = 0;                      ///< Number of times accessed
    double quality_score = 0.0;                   ///< Quality score of cached result
};

/**
 * @brief Sequential strategy for chained model processing
 * 
 * Implements sequential model pipelines with full backward compatibility
 * for the original pipeline concept while adding enhanced features.
 */
class SequentialStrategy {
public:
    /**
     * @brief Constructor
     * @param registry Model registry reference
     * @param config Sequential strategy configuration
     */
    SequentialStrategy(ModelRegistry& registry, 
                      const SequentialStrategyConfig& config = SequentialStrategyConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~SequentialStrategy() = default;
    
    /**
     * @brief Execute sequential request
     * @param request Sequential request
     * @return Sequential response
     */
    virtual SequentialResponse execute(const SequentialRequest& request);
    
    /**
     * @brief Get strategy configuration
     * @return Current configuration
     */
    virtual SequentialStrategyConfig getConfig() const;
    
    /**
     * @brief Set strategy configuration
     * @param config New configuration
     */
    virtual void setConfig(const SequentialStrategyConfig& config);
    
    /**
     * @brief Get strategy statistics
     * @return Current statistics
     */
    virtual SequentialStatistics getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    virtual void resetStatistics();
    
    /**
     * @brief Register pipeline pattern template
     * @param pattern Pattern type
     * @param steps Pipeline steps for pattern
     */
    virtual void registerPatternTemplate(SequentialPattern pattern, 
                                        const std::vector<PipelineStep>& steps);
    
    /**
     * @brief Register named pipeline configuration
     * @param name Pipeline name
     * @param backend_names Backend names for pipeline
     */
    virtual void registerNamedPipeline(const std::string& name, 
                                      const std::vector<std::string>& backend_names);
    
    /**
     * @brief Clear intermediate cache
     */
    virtual void clearCache();
    
    /**
     * @brief Get cache statistics
     * @return Cache hit rate, size, etc.
     */
    virtual std::unordered_map<std::string, double> getCacheStatistics() const;

protected:
    /**
     * @brief Execute original format pipeline
     * @param request Sequential request
     * @param state Pipeline state
     * @return Success status
     */
    virtual bool executeOriginalPipeline(const SequentialRequest& request, PipelineState& state);
    
    /**
     * @brief Execute enhanced format pipeline
     * @param request Sequential request
     * @param state Pipeline state
     * @return Success status
     */
    virtual bool executeEnhancedPipeline(const SequentialRequest& request, PipelineState& state);
    
    /**
     * @brief Execute single pipeline step
     * @param step Pipeline step
     * @param state Pipeline state
     * @return Success status
     */
    virtual bool executeStep(const PipelineStep& step, PipelineState& state);
    
    /**
     * @brief Validate step output
     * @param step Pipeline step
     * @param output Step output
     * @param state Pipeline state
     * @return Validation success
     */
    virtual bool validateStepOutput(const PipelineStep& step, 
                                   const std::string& output, 
                                   PipelineState& state);
    
    /**
     * @brief Handle step failure and potential rollback
     * @param step Failed step
     * @param state Pipeline state
     * @param error_message Error message
     * @return Recovery success
     */
    virtual bool handleStepFailure(const PipelineStep& step, 
                                  PipelineState& state, 
                                  const std::string& error_message);
    
    /**
     * @brief Get pipeline steps for pattern
     * @param pattern Sequential pattern
     * @param request Original request
     * @return Vector of pipeline steps
     */
    virtual std::vector<PipelineStep> getStepsForPattern(SequentialPattern pattern, 
                                                         const SequentialRequest& request);
    
    /**
     * @brief Convert original backend list to pipeline steps
     * @param backend_names Backend names
     * @param request Original request
     * @return Vector of pipeline steps
     */
    virtual std::vector<PipelineStep> convertBackendListToSteps(
        const std::vector<std::string>& backend_names,
        const SequentialRequest& request);
    
    /**
     * @brief Build prompt for step
     * @param step Pipeline step
     * @param state Current pipeline state
     * @return Formatted prompt
     */
    virtual std::string buildStepPrompt(const PipelineStep& step, const PipelineState& state);
    
    /**
     * @brief Check cache for pipeline result
     * @param request Sequential request
     * @param state Pipeline state
     * @return True if cache hit
     */
    virtual bool checkCache(const SequentialRequest& request, PipelineState& state);
    
    /**
     * @brief Store result in cache
     * @param request Sequential request
     * @param state Pipeline state
     */
    virtual void storeInCache(const SequentialRequest& request, const PipelineState& state);
    
    /**
     * @brief Generate cache key for request
     * @param request Sequential request
     * @return Cache key
     */
    virtual std::string generateCacheKey(const SequentialRequest& request);
    
    /**
     * @brief Calculate quality score for step output
     * @param step Pipeline step
     * @param output Step output
     * @param context Context for evaluation
     * @return Quality score (0-1)
     */
    virtual double calculateStepQuality(const PipelineStep& step, 
                                       const std::string& output, 
                                       const std::string& context);
    
    /**
     * @brief Update performance statistics
     * @param request Original request
     * @param response Sequential response
     */
    virtual void updateStatistics(const SequentialRequest& request, 
                                 const SequentialResponse& response);

private:
    ModelRegistry& m_registry;
    SequentialStrategyConfig m_config;
    SequentialStatistics m_statistics;
    
    std::unordered_map<std::string, CachedResult> m_cache;
    
    mutable std::mutex m_stats_mutex;
    mutable std::mutex m_config_mutex;
    mutable std::mutex m_cache_mutex;
    
    /**
     * @brief Initialize default pattern templates
     */
    void initializeDefaultTemplates();
    
    /**
     * @brief Initialize default prompt templates
     */
    void initializeDefaultPrompts();
    
    /**
     * @brief Log original format console output
     * @param step_number Current step number
     * @param total_steps Total number of steps
     * @param model_name Model being used
     * @param response Response to add to log
     */
    void logOriginalFormatOutput(size_t step_number, size_t total_steps, 
                                const std::string& model_name, 
                                SequentialResponse& response);
    
    /**
     * @brief Cleanup expired cache entries
     */
    void cleanupCache();
    
    /**
     * @brief Apply prompt template with variable substitution
     * @param template_str Template string
     * @param variables Variable map
     * @return Formatted string
     */
    std::string applyTemplate(const std::string& template_str, 
                             const std::unordered_map<std::string, std::string>& variables);
    
    /**
     * @brief Calculate overall pipeline quality
     * @param state Pipeline state
     * @return Overall quality score
     */
    double calculateOverallQuality(const PipelineState& state);
    
    /**
     * @brief Get default quality scorer function
     * @param context Context for scoring
     * @return Quality scorer function
     */
    std::function<double(const std::string&)> getQualityScorer(const std::string& context);
};

/**
 * @brief Convert sequential pattern to string representation
 * @param pattern Sequential pattern
 * @return String representation
 */
std::string sequentialPatternToString(SequentialPattern pattern);

/**
 * @brief Convert string to sequential pattern
 * @param str String representation
 * @return Sequential pattern
 */
SequentialPattern stringToSequentialPattern(const std::string& str);

} // namespace Camus