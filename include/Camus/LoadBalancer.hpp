// =================================================================
// include/Camus/LoadBalancer.hpp
// =================================================================
// Load balancing system for distributing requests across multiple model instances.

#pragma once

#include "Camus/ModelRegistry.hpp"
#include "Camus/LlmInteraction.hpp"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

namespace Camus {

/**
 * @brief Model instance information
 */
struct ModelInstance {
    std::string instance_id;                      ///< Unique instance identifier
    std::string model_name;                       ///< Name of the model
    std::shared_ptr<LlmInteraction> model;        ///< Model implementation
    std::atomic<bool> is_healthy{true};           ///< Health status
    std::atomic<size_t> active_requests{0};       ///< Current active requests
    std::atomic<double> average_response_time{0.0}; ///< Average response time in ms
    std::atomic<size_t> total_requests{0};        ///< Total requests handled
    std::atomic<size_t> failed_requests{0};       ///< Failed requests count
    std::chrono::system_clock::time_point last_health_check; ///< Last health check time
    std::chrono::system_clock::time_point created_at; ///< Instance creation time
    std::chrono::system_clock::time_point last_used; ///< Last usage time
    double max_memory_usage = 0.0;                ///< Maximum memory usage in GB
    double current_memory_usage = 0.0;            ///< Current memory usage in GB
};

/**
 * @brief Load balancing strategy type
 */
enum class LoadBalancingStrategy {
    ROUND_ROBIN,        ///< Distribute requests in round-robin fashion
    LEAST_LOADED,       ///< Send to instance with fewest active requests
    RESPONSE_TIME,      ///< Send to instance with best response time
    RESOURCE_USAGE,     ///< Send to instance with lowest resource usage
    WEIGHTED_ROUND_ROBIN ///< Round-robin with instance weights
};

/**
 * @brief Request context for load balancing
 */
struct RequestContext {
    std::string request_id;                       ///< Unique request identifier
    std::string prompt;                           ///< Request prompt
    size_t estimated_tokens = 0;                  ///< Estimated token count
    std::chrono::milliseconds max_response_time{30000}; ///< Maximum allowed response time
    int priority = 0;                             ///< Request priority (higher = more urgent)
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
};

/**
 * @brief Load balancing result
 */
struct LoadBalancingResult {
    std::string selected_instance_id;            ///< Selected instance ID
    std::shared_ptr<LlmInteraction> model;       ///< Selected model instance
    std::string selection_reason;                ///< Reason for selection
    std::chrono::milliseconds selection_time{0}; ///< Time taken for selection
    std::vector<std::string> alternative_instances; ///< Alternative instances
    bool fallback_used = false;                  ///< Whether fallback was used
};

/**
 * @brief Load balancer configuration
 */
struct LoadBalancerConfig {
    LoadBalancingStrategy default_strategy = LoadBalancingStrategy::LEAST_LOADED;
    size_t max_instances_per_model = 3;          ///< Maximum instances per model
    size_t max_requests_per_instance = 10;       ///< Maximum concurrent requests per instance
    std::chrono::minutes health_check_interval{2}; ///< Health check frequency
    std::chrono::seconds instance_timeout{300};  ///< Instance inactivity timeout
    std::chrono::seconds request_timeout{30};    ///< Default request timeout
    bool auto_scale = true;                       ///< Enable automatic scaling
    bool enable_fallback = true;                  ///< Enable fallback instances
    double memory_usage_threshold = 0.8;         ///< Memory usage threshold for scaling
    double response_time_threshold = 5000.0;     ///< Response time threshold in ms
    std::unordered_map<std::string, double> instance_weights; ///< Instance weights for weighted round-robin
};

/**
 * @brief Load balancing strategy base class
 */
class BalancingStrategy {
public:
    virtual ~BalancingStrategy() = default;
    
    /**
     * @brief Select best instance for request
     * @param context Request context
     * @param instances Available instances
     * @return Selected instance ID or empty if none available
     */
    virtual std::string selectInstance(
        const RequestContext& context,
        const std::vector<ModelInstance*>& instances
    ) = 0;
    
    /**
     * @brief Get strategy name
     * @return Strategy identifier
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Update strategy state after request completion
     * @param instance_id Instance that handled the request
     * @param response_time Response time in milliseconds
     * @param success Whether request succeeded
     */
    virtual void updateState(const std::string& instance_id, double response_time, bool success) {}
};

/**
 * @brief Load balancer for distributing requests across model instances
 */
class LoadBalancer {
public:
    /**
     * @brief Constructor
     * @param registry Model registry reference
     * @param config Load balancer configuration
     */
    LoadBalancer(ModelRegistry& registry, const LoadBalancerConfig& config = LoadBalancerConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~LoadBalancer();
    
    /**
     * @brief Select best model instance for request
     * @param model_name Model name to load balance
     * @param context Request context
     * @return Load balancing result
     */
    virtual LoadBalancingResult selectInstance(const std::string& model_name, const RequestContext& context);
    
    /**
     * @brief Create new model instance
     * @param model_name Model name
     * @return Instance ID if successful, empty string if failed
     */
    virtual std::string createInstance(const std::string& model_name);
    
    /**
     * @brief Remove model instance
     * @param instance_id Instance to remove
     * @return True if successfully removed
     */
    virtual bool removeInstance(const std::string& instance_id);
    
    /**
     * @brief Get all instances for a model
     * @param model_name Model name
     * @return Vector of instance pointers
     */
    virtual std::vector<ModelInstance*> getInstancesForModel(const std::string& model_name);
    
    /**
     * @brief Get instance by ID
     * @param instance_id Instance identifier
     * @return Instance pointer or nullptr if not found
     */
    virtual ModelInstance* getInstance(const std::string& instance_id);
    
    /**
     * @brief Register load balancing strategy
     * @param strategy Strategy type
     * @param implementation Strategy implementation
     */
    virtual void registerStrategy(LoadBalancingStrategy strategy, std::shared_ptr<BalancingStrategy> implementation);
    
    /**
     * @brief Set active load balancing strategy
     * @param strategy Strategy to use
     */
    virtual void setStrategy(LoadBalancingStrategy strategy);
    
    /**
     * @brief Get current load balancing strategy
     * @return Current strategy
     */
    virtual LoadBalancingStrategy getStrategy() const;
    
    /**
     * @brief Record request start
     * @param instance_id Instance handling the request
     * @param request_id Request identifier
     */
    virtual void recordRequestStart(const std::string& instance_id, const std::string& request_id);
    
    /**
     * @brief Record request completion
     * @param instance_id Instance that handled the request
     * @param request_id Request identifier
     * @param response_time Response time in milliseconds
     * @param success Whether request succeeded
     */
    virtual void recordRequestEnd(const std::string& instance_id, const std::string& request_id, 
                                double response_time, bool success);
    
    /**
     * @brief Perform health checks on all instances
     * @return Number of healthy instances
     */
    virtual size_t performHealthChecks();
    
    /**
     * @brief Get load balancer statistics
     * @return Statistics as formatted string
     */
    virtual std::string getStatistics() const;
    
    /**
     * @brief Scale instances based on load
     * @param model_name Model to scale
     * @return Number of instances after scaling
     */
    virtual size_t autoScale(const std::string& model_name);
    
    /**
     * @brief Clean up inactive instances
     * @return Number of instances removed
     */
    virtual size_t cleanupInactiveInstances();
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    virtual LoadBalancerConfig getConfig() const;
    
    /**
     * @brief Set configuration
     * @param config New configuration
     */
    virtual void setConfig(const LoadBalancerConfig& config);
    
    /**
     * @brief Enable/disable automatic scaling
     * @param enabled Whether to enable auto-scaling
     */
    virtual void setAutoScaling(bool enabled);
    
    /**
     * @brief Get total number of instances
     * @return Total instance count
     */
    virtual size_t getTotalInstances() const;
    
    /**
     * @brief Get number of healthy instances
     * @return Healthy instance count
     */
    virtual size_t getHealthyInstances() const;

protected:
    /**
     * @brief Generate unique instance ID
     * @param model_name Model name
     * @return Unique instance identifier
     */
    virtual std::string generateInstanceId(const std::string& model_name);
    
    /**
     * @brief Check if instance needs scaling
     * @param model_name Model name
     * @return True if scaling is needed
     */
    virtual bool needsScaling(const std::string& model_name);
    
    /**
     * @brief Start background health check thread
     */
    void startHealthCheckThread();
    
    /**
     * @brief Stop background health check thread
     */
    void stopHealthCheckThread();
    
    /**
     * @brief Health check thread function
     */
    void healthCheckLoop();

private:
    ModelRegistry& m_registry;
    LoadBalancerConfig m_config;
    LoadBalancingStrategy m_current_strategy;
    
    std::unordered_map<std::string, std::unique_ptr<ModelInstance>> m_instances;
    std::unordered_map<std::string, std::vector<std::string>> m_model_instances;
    std::unordered_map<LoadBalancingStrategy, std::shared_ptr<BalancingStrategy>> m_strategies;
    std::unordered_map<std::string, std::string> m_active_requests; // request_id -> instance_id
    
    mutable std::mutex m_instances_mutex;
    mutable std::mutex m_requests_mutex;
    
    std::unique_ptr<std::thread> m_health_check_thread;
    std::atomic<bool> m_stop_health_checks{false};
    
    std::atomic<size_t> m_next_instance_id{1};
    
    // Built-in strategy classes
    class RoundRobinStrategy;
    class LeastLoadedStrategy;
    class ResponseTimeStrategy;
    class ResourceUsageStrategy;
    class WeightedRoundRobinStrategy;
};

} // namespace Camus