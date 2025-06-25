// =================================================================
// include/Camus/ParallelStrategy.hpp
// =================================================================
// Parallel strategy for concurrent model execution on independent subtasks.

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
#include <atomic>
#include <thread>
#include <future>

namespace Camus {

/**
 * @brief Parallel execution pattern type
 */
enum class ParallelPattern {
    FILE_ANALYSIS,          ///< Analyze multiple files simultaneously
    MULTI_ASPECT_REVIEW,    ///< Security, performance, style checks in parallel
    ALTERNATIVE_GENERATION, ///< Generate multiple solution approaches
    VALIDATION_SUITE,       ///< Run multiple validation checks concurrently
    CUSTOM                  ///< Custom user-defined pattern
};

/**
 * @brief Subtask definition for parallel execution
 */
struct ParallelSubtask {
    std::string subtask_id;                       ///< Unique subtask identifier
    std::string model_name;                       ///< Model to use for this subtask
    std::string description;                      ///< Human-readable description
    std::string prompt;                           ///< Prompt for the subtask
    std::unordered_map<std::string, std::string> parameters; ///< Subtask parameters
    bool is_critical = false;                     ///< Whether failure fails entire request
    std::chrono::milliseconds timeout{30000};     ///< Subtask-specific timeout
    double weight = 1.0;                          ///< Weight for result aggregation
    std::vector<std::string> dependencies;        ///< Other subtask IDs this depends on
    std::string aggregation_group;                ///< Group for result aggregation
};

/**
 * @brief Result from a single subtask
 */
struct SubtaskResult {
    std::string subtask_id;                       ///< Subtask identifier
    std::string model_used;                       ///< Model that processed this
    std::string result_text;                      ///< Result output
    bool success = false;                         ///< Whether subtask succeeded
    std::string error_message;                    ///< Error if failed
    std::chrono::milliseconds execution_time{0};  ///< Time to execute
    double quality_score = 0.0;                   ///< Quality of result
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
};

/**
 * @brief Aggregation method for combining parallel results
 */
enum class AggregationMethod {
    CONCATENATE,        ///< Simple concatenation of results
    MERGE,              ///< Intelligent merging with deduplication
    BEST_QUALITY,       ///< Select highest quality result
    CONSENSUS,          ///< Find consensus among results
    WEIGHTED_COMBINE,   ///< Weighted combination based on quality
    CUSTOM              ///< Custom aggregation function
};

/**
 * @brief Conflict resolution strategy
 */
enum class ConflictResolution {
    MAJORITY_VOTE,      ///< Use majority consensus
    HIGHEST_CONFIDENCE, ///< Use result with highest confidence
    MANUAL_REVIEW,      ///< Flag for manual review
    MERGE_ALL,          ///< Attempt to merge all perspectives
    FIRST_VALID        ///< Use first valid result
};

/**
 * @brief Parallel request structure
 */
struct ParallelRequest {
    std::string request_id;                       ///< Unique request identifier
    std::string prompt;                           ///< User prompt
    std::string context;                          ///< Additional context
    TaskType task_type = TaskType::UNKNOWN;       ///< Classified task type
    ParallelPattern pattern = ParallelPattern::CUSTOM; ///< Execution pattern
    
    std::vector<ParallelSubtask> subtasks;       ///< Subtasks to execute
    AggregationMethod aggregation_method = AggregationMethod::MERGE; ///< How to combine results
    ConflictResolution conflict_strategy = ConflictResolution::MAJORITY_VOTE; ///< Conflict handling
    
    std::chrono::milliseconds timeout{120000};    ///< Total request timeout
    size_t max_concurrent_tasks = 4;              ///< Maximum parallel executions
    bool enable_resource_limits = true;           ///< Enable resource management
    double min_success_ratio = 0.5;               ///< Minimum ratio of successful subtasks
    
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
    std::function<std::string(const std::vector<SubtaskResult>&)> custom_aggregator; ///< Custom aggregation
};

/**
 * @brief Parallel response structure
 */
struct ParallelResponse {
    std::string request_id;                       ///< Original request ID
    std::string aggregated_result;                ///< Final aggregated result
    bool success = false;                         ///< Whether request succeeded
    std::string error_message;                    ///< Error message if failed
    
    ParallelPattern pattern_used;                 ///< Pattern that was used
    std::vector<SubtaskResult> subtask_results;  ///< Individual subtask results
    
    std::chrono::milliseconds total_time{0};      ///< Total processing time
    std::chrono::milliseconds parallel_time{0};   ///< Time for parallel execution
    std::chrono::milliseconds aggregation_time{0}; ///< Time for result aggregation
    
    size_t subtasks_completed = 0;                ///< Number of completed subtasks
    size_t subtasks_succeeded = 0;                ///< Number of successful subtasks
    size_t total_subtasks = 0;                    ///< Total number of subtasks
    
    double overall_quality_score = 0.0;           ///< Overall quality of aggregated result
    std::vector<std::string> conflict_notes;      ///< Notes about resolved conflicts
    std::unordered_map<std::string, size_t> resource_usage; ///< Resource usage statistics
    
    double speedup_factor = 1.0;                  ///< Speedup vs sequential execution
    std::unordered_map<std::string, std::string> aggregation_details; ///< Details of aggregation
};

/**
 * @brief Configuration for ParallelStrategy
 */
struct ParallelStrategyConfig {
    size_t max_concurrent_executions = 8;         ///< Maximum parallel model executions
    size_t thread_pool_size = 0;                  ///< Thread pool size (0 = auto)
    bool enable_resource_monitoring = true;       ///< Monitor CPU/memory usage
    bool enable_adaptive_throttling = true;       ///< Dynamically adjust concurrency
    
    double max_cpu_usage_percent = 80.0;          ///< Maximum CPU usage threshold
    double max_memory_usage_percent = 70.0;       ///< Maximum memory usage threshold
    
    std::chrono::milliseconds default_subtask_timeout{30000}; ///< Default timeout per subtask
    std::chrono::milliseconds max_total_time{300000}; ///< Maximum total execution time
    
    bool enable_dependency_resolution = true;      ///< Handle subtask dependencies
    bool enable_failure_recovery = true;          ///< Retry failed non-critical tasks
    size_t max_retry_attempts = 2;                ///< Maximum retries per subtask
    
    std::unordered_map<ParallelPattern, std::vector<ParallelSubtask>> pattern_templates; ///< Templates
    std::unordered_map<AggregationMethod, std::function<std::string(const std::vector<SubtaskResult>&)>> 
        aggregation_functions; ///< Custom aggregation functions
    
    bool enable_performance_tracking = true;       ///< Track performance metrics
    bool enable_conflict_logging = true;          ///< Log conflict resolutions
};

/**
 * @brief Performance statistics for parallel strategy
 */
struct ParallelStatistics {
    size_t total_requests = 0;                    ///< Total parallel requests
    size_t successful_requests = 0;               ///< Successful requests
    size_t failed_requests = 0;                   ///< Failed requests
    
    double average_speedup_factor = 1.0;          ///< Average speedup achieved
    double average_parallel_efficiency = 0.0;     ///< Parallel execution efficiency
    double average_resource_utilization = 0.0;    ///< Resource usage efficiency
    
    size_t total_subtasks_executed = 0;           ///< Total subtasks executed
    size_t total_subtasks_succeeded = 0;          ///< Total successful subtasks
    size_t total_conflicts_resolved = 0;          ///< Total conflicts resolved
    
    std::unordered_map<ParallelPattern, size_t> pattern_usage; ///< Usage by pattern
    std::unordered_map<AggregationMethod, size_t> aggregation_usage; ///< Aggregation methods used
    std::unordered_map<std::string, double> model_subtask_performance; ///< Model performance
    
    double peak_concurrent_executions = 0.0;      ///< Peak concurrent tasks
    double average_concurrent_executions = 0.0;   ///< Average concurrent tasks
    
    std::chrono::system_clock::time_point last_reset; ///< Last statistics reset
};

/**
 * @brief Resource usage snapshot
 */
struct ResourceUsage {
    double cpu_usage_percent = 0.0;              ///< Current CPU usage
    double memory_usage_percent = 0.0;            ///< Current memory usage
    size_t active_threads = 0;                    ///< Active thread count
    size_t queued_tasks = 0;                      ///< Tasks waiting in queue
    std::chrono::system_clock::time_point timestamp; ///< Measurement time
};

/**
 * @brief Parallel strategy for concurrent model execution
 * 
 * Enables efficient parallel processing of independent subtasks
 * with intelligent result aggregation and resource management.
 */
class ParallelStrategy {
public:
    /**
     * @brief Constructor
     * @param registry Model registry reference
     * @param config Parallel strategy configuration
     */
    ParallelStrategy(ModelRegistry& registry, 
                    const ParallelStrategyConfig& config = ParallelStrategyConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~ParallelStrategy();
    
    /**
     * @brief Execute parallel request
     * @param request Parallel request
     * @return Parallel response
     */
    virtual ParallelResponse execute(const ParallelRequest& request);
    
    /**
     * @brief Get strategy configuration
     * @return Current configuration
     */
    virtual ParallelStrategyConfig getConfig() const;
    
    /**
     * @brief Set strategy configuration
     * @param config New configuration
     */
    virtual void setConfig(const ParallelStrategyConfig& config);
    
    /**
     * @brief Get strategy statistics
     * @return Current statistics
     */
    virtual ParallelStatistics getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    virtual void resetStatistics();
    
    /**
     * @brief Get current resource usage
     * @return Resource usage snapshot
     */
    virtual ResourceUsage getCurrentResourceUsage() const;
    
    /**
     * @brief Register pattern template
     * @param pattern Pattern type
     * @param subtasks Subtask definitions for pattern
     */
    virtual void registerPatternTemplate(ParallelPattern pattern, 
                                       const std::vector<ParallelSubtask>& subtasks);
    
    /**
     * @brief Register custom aggregation function
     * @param method Aggregation method
     * @param aggregator Aggregation function
     */
    virtual void registerAggregationFunction(
        AggregationMethod method,
        std::function<std::string(const std::vector<SubtaskResult>&)> aggregator);
    
    /**
     * @brief Stop all active executions
     */
    virtual void shutdown();

protected:
    /**
     * @brief Execute subtasks in parallel
     * @param request Parallel request
     * @param subtasks Subtasks to execute
     * @return Vector of subtask results
     */
    virtual std::vector<SubtaskResult> executeSubtasks(
        const ParallelRequest& request,
        const std::vector<ParallelSubtask>& subtasks);
    
    /**
     * @brief Execute single subtask
     * @param subtask Subtask definition
     * @param request Original request for context
     * @return Subtask result
     */
    virtual SubtaskResult executeSubtask(const ParallelSubtask& subtask,
                                       const ParallelRequest& request);
    
    /**
     * @brief Aggregate subtask results
     * @param results Subtask results
     * @param method Aggregation method
     * @param request Original request
     * @return Aggregated result string
     */
    virtual std::string aggregateResults(const std::vector<SubtaskResult>& results,
                                       AggregationMethod method,
                                       const ParallelRequest& request);
    
    /**
     * @brief Resolve conflicts in results
     * @param results Subtask results
     * @param strategy Conflict resolution strategy
     * @param conflicts Output conflict notes
     * @return Resolved results
     */
    virtual std::vector<SubtaskResult> resolveConflicts(
        const std::vector<SubtaskResult>& results,
        ConflictResolution strategy,
        std::vector<std::string>& conflicts);
    
    /**
     * @brief Get subtasks for pattern
     * @param pattern Parallel pattern
     * @param request Original request
     * @return Vector of subtasks
     */
    virtual std::vector<ParallelSubtask> getSubtasksForPattern(
        ParallelPattern pattern,
        const ParallelRequest& request);
    
    /**
     * @brief Check if resource limits allow execution
     * @return True if resources available
     */
    virtual bool checkResourceAvailability();
    
    /**
     * @brief Wait for resources to become available
     * @param timeout Maximum wait time
     * @return True if resources became available
     */
    virtual bool waitForResources(std::chrono::milliseconds timeout);
    
    /**
     * @brief Update resource usage statistics
     * @param subtask_count Number of active subtasks
     */
    virtual void updateResourceUsage(size_t subtask_count);
    
    /**
     * @brief Calculate quality score for aggregated result
     * @param aggregated_result Aggregated result text
     * @param subtask_results Individual results
     * @return Quality score (0-1)
     */
    virtual double calculateAggregatedQuality(const std::string& aggregated_result,
                                            const std::vector<SubtaskResult>& subtask_results);
    
    /**
     * @brief Update performance statistics
     * @param request Original request
     * @param response Parallel response
     */
    virtual void updateStatistics(const ParallelRequest& request,
                                const ParallelResponse& response);

private:
    ModelRegistry& m_registry;
    ParallelStrategyConfig m_config;
    ParallelStatistics m_statistics;
    
    // Thread pool for parallel execution
    class ThreadPool;
    std::unique_ptr<ThreadPool> m_thread_pool;
    
    // Resource monitoring
    std::atomic<double> m_current_cpu_usage{0.0};
    std::atomic<double> m_current_memory_usage{0.0};
    std::atomic<size_t> m_active_executions{0};
    
    mutable std::mutex m_stats_mutex;
    mutable std::mutex m_config_mutex;
    std::atomic<bool> m_shutdown_requested{false};
    
    /**
     * @brief Initialize thread pool
     */
    void initializeThreadPool();
    
    /**
     * @brief Initialize default templates
     */
    void initializeDefaultTemplates();
    
    /**
     * @brief Initialize default aggregation functions
     */
    void initializeDefaultAggregators();
    
    /**
     * @brief Monitor system resources
     */
    void monitorResources();
    
    /**
     * @brief Resolve subtask dependencies
     * @param subtasks Original subtasks
     * @return Ordered subtasks respecting dependencies
     */
    std::vector<std::vector<ParallelSubtask>> resolveDependencies(
        const std::vector<ParallelSubtask>& subtasks);
    
    /**
     * @brief Default concatenation aggregator
     * @param results Subtask results
     * @return Concatenated results
     */
    static std::string concatenateAggregator(const std::vector<SubtaskResult>& results);
    
    /**
     * @brief Default merge aggregator
     * @param results Subtask results
     * @return Merged results
     */
    static std::string mergeAggregator(const std::vector<SubtaskResult>& results);
    
    /**
     * @brief Default best quality aggregator
     * @param results Subtask results
     * @return Best quality result
     */
    static std::string bestQualityAggregator(const std::vector<SubtaskResult>& results);
    
    /**
     * @brief Default consensus aggregator
     * @param results Subtask results
     * @return Consensus result
     */
    static std::string consensusAggregator(const std::vector<SubtaskResult>& results);
    
    /**
     * @brief Default weighted combine aggregator
     * @param results Subtask results
     * @return Weighted combination
     */
    static std::string weightedCombineAggregator(const std::vector<SubtaskResult>& results);
};

/**
 * @brief Convert parallel pattern to string representation
 * @param pattern Parallel pattern
 * @return String representation
 */
std::string parallelPatternToString(ParallelPattern pattern);

/**
 * @brief Convert string to parallel pattern
 * @param str String representation
 * @return Parallel pattern
 */
ParallelPattern stringToParallelPattern(const std::string& str);

/**
 * @brief Convert aggregation method to string representation
 * @param method Aggregation method
 * @return String representation
 */
std::string aggregationMethodToString(AggregationMethod method);

/**
 * @brief Convert string to aggregation method
 * @param str String representation
 * @return Aggregation method
 */
AggregationMethod stringToAggregationMethod(const std::string& str);

} // namespace Camus