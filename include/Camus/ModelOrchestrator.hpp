// =================================================================
// include/Camus/ModelOrchestrator.hpp
// =================================================================
// Main coordination class for request routing pipeline combining classification, selection, and load balancing.

#pragma once

#include "Camus/TaskClassifier.hpp"
#include "Camus/ModelSelector.hpp"
#include "Camus/LoadBalancer.hpp"
#include "Camus/ModelRegistry.hpp"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>

namespace Camus {

/**
 * @brief Pipeline request structure
 */
struct PipelineRequest {
    std::string request_id;                       ///< Unique request identifier
    std::string prompt;                           ///< User prompt
    std::string context;                          ///< Additional context
    int max_tokens = 2048;                        ///< Maximum output tokens
    double temperature = 0.7;                     ///< Sampling temperature
    int priority = 0;                             ///< Request priority (higher = more urgent)
    std::chrono::milliseconds timeout{30000};    ///< Request timeout
    bool enable_caching = true;                   ///< Whether to use cache
    bool require_fallback = true;                 ///< Enable fallback on failure
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
    std::vector<std::string> preferred_models;    ///< Preferred model order
    std::vector<std::string> excluded_models;     ///< Models to exclude
};

/**
 * @brief Pipeline response structure
 */
struct PipelineResponse {
    std::string request_id;                       ///< Original request ID
    std::string response_text;                    ///< Generated response
    bool success = false;                         ///< Whether request succeeded
    std::string error_message;                    ///< Error message if failed
    std::string selected_model;                   ///< Model that generated response
    std::string selected_instance;               ///< Instance that handled request
    TaskType classified_task;                     ///< Classified task type
    double classification_confidence = 0.0;       ///< Classification confidence
    double selection_confidence = 0.0;            ///< Model selection confidence
    std::chrono::milliseconds total_time{0};     ///< Total processing time
    std::chrono::milliseconds classification_time{0}; ///< Time for task classification
    std::chrono::milliseconds selection_time{0}; ///< Time for model selection
    std::chrono::milliseconds execution_time{0}; ///< Time for model execution
    size_t tokens_generated = 0;                  ///< Number of tokens generated
    bool cache_hit = false;                       ///< Whether response came from cache
    bool fallback_used = false;                   ///< Whether fallback was triggered
    double quality_score = 0.0;                   ///< Response quality score (0-1)
    std::vector<std::string> pipeline_steps;      ///< Steps taken in pipeline
    std::unordered_map<std::string, std::string> debug_info; ///< Debug information
};

/**
 * @brief Cached response entry
 */
struct CacheEntry {
    std::string response_text;                    ///< Cached response
    std::string model_used;                       ///< Model that generated response
    std::chrono::system_clock::time_point created_at; ///< Cache creation time
    std::chrono::system_clock::time_point last_accessed; ///< Last access time
    size_t access_count = 0;                      ///< Number of times accessed
    double quality_score = 0.0;                   ///< Response quality score
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
};

/**
 * @brief Orchestrator configuration
 */
struct OrchestratorConfig {
    bool enable_classification = true;            ///< Enable task classification
    bool enable_model_selection = true;           ///< Enable intelligent model selection
    bool enable_load_balancing = true;            ///< Enable load balancing
    bool enable_caching = true;                   ///< Enable response caching
    bool enable_fallback = true;                  ///< Enable fallback mechanisms
    bool enable_quality_checks = true;            ///< Enable response quality validation
    
    // Performance settings
    std::chrono::seconds cache_ttl{3600};         ///< Cache time-to-live (1 hour)
    size_t max_cache_size = 1000;                 ///< Maximum cache entries
    std::chrono::milliseconds default_timeout{30000}; ///< Default request timeout
    size_t max_retries = 3;                       ///< Maximum retry attempts
    std::chrono::milliseconds retry_delay{1000};  ///< Delay between retries
    
    // Quality thresholds
    double min_quality_score = 0.3;               ///< Minimum acceptable quality
    double min_classification_confidence = 0.5;   ///< Minimum classification confidence
    double min_selection_confidence = 0.3;        ///< Minimum selection confidence
    
    // Fallback settings
    std::string fallback_model = "";              ///< Default fallback model
    bool fallback_on_timeout = true;              ///< Use fallback on timeout
    bool fallback_on_error = true;                ///< Use fallback on error
    bool fallback_on_low_quality = false;         ///< Use fallback on low quality
    
    // Cache settings
    bool cache_negative_responses = false;        ///< Cache failed responses
    size_t min_prompt_length_for_cache = 10;      ///< Minimum prompt length to cache
    double cache_similarity_threshold = 0.95;     ///< Similarity threshold for cache hits
};

/**
 * @brief Request routing pipeline statistics
 */
struct PipelineStatistics {
    size_t total_requests = 0;                    ///< Total requests processed
    size_t successful_requests = 0;               ///< Successful requests
    size_t failed_requests = 0;                   ///< Failed requests
    size_t cache_hits = 0;                        ///< Cache hit count
    size_t fallback_used = 0;                     ///< Fallback usage count
    double average_response_time = 0.0;           ///< Average response time in ms
    double average_quality_score = 0.0;           ///< Average quality score
    std::unordered_map<std::string, size_t> model_usage; ///< Model usage counts
    std::unordered_map<TaskType, size_t> task_distribution; ///< Task type distribution
    std::chrono::system_clock::time_point last_reset; ///< Last statistics reset
};

/**
 * @brief Fallback strategy type
 */
enum class FallbackStrategy {
    NONE,                    ///< No fallback
    SIMPLE_MODEL,           ///< Fall back to simpler model
    FASTEST_MODEL,          ///< Fall back to fastest available model
    CACHED_RESPONSE,        ///< Use cached similar response
    DEFAULT_RESPONSE        ///< Use predefined default response
};

/**
 * @brief Main orchestrator for request routing pipeline
 */
class ModelOrchestrator {
public:
    /**
     * @brief Constructor
     * @param registry Model registry reference
     * @param config Orchestrator configuration
     */
    ModelOrchestrator(ModelRegistry& registry, const OrchestratorConfig& config = OrchestratorConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~ModelOrchestrator();
    
    /**
     * @brief Process a request through the complete pipeline
     * @param request Pipeline request
     * @return Pipeline response
     */
    virtual PipelineResponse processRequest(const PipelineRequest& request);
    
    /**
     * @brief Process multiple requests asynchronously
     * @param requests Vector of pipeline requests
     * @return Vector of pipeline responses
     */
    virtual std::vector<PipelineResponse> processRequests(const std::vector<PipelineRequest>& requests);
    
    /**
     * @brief Get orchestrator configuration
     * @return Current configuration
     */
    virtual OrchestratorConfig getConfig() const;
    
    /**
     * @brief Set orchestrator configuration
     * @param config New configuration
     */
    virtual void setConfig(const OrchestratorConfig& config);
    
    /**
     * @brief Get pipeline statistics
     * @return Current statistics
     */
    virtual PipelineStatistics getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    virtual void resetStatistics();
    
    /**
     * @brief Clear response cache
     */
    virtual void clearCache();
    
    /**
     * @brief Get cache statistics
     * @return Cache hit rate, size, etc.
     */
    virtual std::unordered_map<std::string, double> getCacheStatistics() const;
    
    /**
     * @brief Set fallback strategy
     * @param strategy Fallback strategy to use
     */
    virtual void setFallbackStrategy(FallbackStrategy strategy);
    
    /**
     * @brief Get current fallback strategy
     * @return Current fallback strategy
     */
    virtual FallbackStrategy getFallbackStrategy() const;
    
    /**
     * @brief Register custom quality scorer
     * @param scorer Function to score response quality (0-1)
     */
    virtual void registerQualityScorer(std::function<double(const std::string&, const std::string&)> scorer);
    
    /**
     * @brief Warm up the pipeline with common requests
     * @param sample_requests Vector of sample requests for warmup
     */
    virtual void warmupPipeline(const std::vector<std::string>& sample_requests);
    
    /**
     * @brief Get detailed performance metrics
     * @return Formatted performance report
     */
    virtual std::string getPerformanceReport() const;
    
    /**
     * @brief Enable/disable specific pipeline components
     * @param component Component name
     * @param enabled Whether to enable the component
     */
    virtual void setComponentEnabled(const std::string& component, bool enabled);

protected:
    /**
     * @brief Classify the task type
     * @param request Pipeline request
     * @param response Pipeline response (to update)
     * @return Classification result
     */
    virtual ClassificationResult classifyTask(const PipelineRequest& request, PipelineResponse& response);
    
    /**
     * @brief Select appropriate model
     * @param request Pipeline request
     * @param task_result Classification result
     * @param response Pipeline response (to update)
     * @return Selection result
     */
    virtual SelectionResult selectModel(const PipelineRequest& request, 
                                      const ClassificationResult& task_result,
                                      PipelineResponse& response);
    
    /**
     * @brief Select model instance for load balancing
     * @param request Pipeline request
     * @param model_name Selected model name
     * @param response Pipeline response (to update)
     * @return Load balancing result
     */
    virtual LoadBalancingResult selectInstance(const PipelineRequest& request,
                                             const std::string& model_name,
                                             PipelineResponse& response);
    
    /**
     * @brief Execute request on selected model instance
     * @param request Pipeline request
     * @param lb_result Load balancing result
     * @param response Pipeline response (to update)
     * @return True if execution succeeded
     */
    virtual bool executeRequest(const PipelineRequest& request,
                              const LoadBalancingResult& lb_result,
                              PipelineResponse& response);
    
    /**
     * @brief Validate and score response quality
     * @param request Pipeline request
     * @param response Pipeline response (to update)
     * @return Quality score (0-1)
     */
    virtual double validateResponse(const PipelineRequest& request, PipelineResponse& response);
    
    /**
     * @brief Handle fallback when primary execution fails
     * @param request Pipeline request
     * @param response Pipeline response (to update)
     * @return True if fallback succeeded
     */
    virtual bool handleFallback(const PipelineRequest& request, PipelineResponse& response);
    
    /**
     * @brief Check cache for similar request
     * @param request Pipeline request
     * @param response Pipeline response (to update)
     * @return True if cache hit
     */
    virtual bool checkCache(const PipelineRequest& request, PipelineResponse& response);
    
    /**
     * @brief Store response in cache
     * @param request Pipeline request
     * @param response Pipeline response
     */
    virtual void storeInCache(const PipelineRequest& request, const PipelineResponse& response);
    
    /**
     * @brief Generate cache key for request
     * @param request Pipeline request
     * @return Cache key string
     */
    virtual std::string generateCacheKey(const PipelineRequest& request);
    
    /**
     * @brief Calculate similarity between two prompts
     * @param prompt1 First prompt
     * @param prompt2 Second prompt
     * @return Similarity score (0-1)
     */
    virtual double calculatePromptSimilarity(const std::string& prompt1, const std::string& prompt2);
    
    /**
     * @brief Clean up expired cache entries
     */
    virtual void cleanupCache();
    
    /**
     * @brief Update statistics with request result
     * @param request Pipeline request
     * @param response Pipeline response
     */
    virtual void updateStatistics(const PipelineRequest& request, const PipelineResponse& response);

private:
    ModelRegistry& m_registry;
    OrchestratorConfig m_config;
    FallbackStrategy m_fallback_strategy = FallbackStrategy::SIMPLE_MODEL;
    
    std::unique_ptr<TaskClassifier> m_classifier;
    std::unique_ptr<ModelSelector> m_selector;
    std::unique_ptr<LoadBalancer> m_load_balancer;
    
    std::unordered_map<std::string, CacheEntry> m_cache;
    PipelineStatistics m_statistics;
    
    mutable std::mutex m_cache_mutex;
    mutable std::mutex m_stats_mutex;
    
    std::function<double(const std::string&, const std::string&)> m_quality_scorer;
    
    std::unique_ptr<std::thread> m_cleanup_thread;
    std::atomic<bool> m_stop_cleanup{false};
    
    /**
     * @brief Cache cleanup thread function
     */
    void cacheCleanupLoop();
    
    /**
     * @brief Start background cleanup thread
     */
    void startCleanupThread();
    
    /**
     * @brief Stop background cleanup thread
     */
    void stopCleanupThread();
    
    /**
     * @brief Default quality scoring function
     */
    double defaultQualityScorer(const std::string& prompt, const std::string& response);
};

} // namespace Camus