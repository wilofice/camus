// =================================================================
// src/Camus/LoadBalancer.cpp
// =================================================================
// Implementation of load balancing system.

#include "Camus/LoadBalancer.hpp"
#include "Camus/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

namespace Camus {

// =================================================================
// Built-in Load Balancing Strategies
// =================================================================

/**
 * @brief Round-robin load balancing strategy
 */
class LoadBalancer::RoundRobinStrategy : public BalancingStrategy {
private:
    std::unordered_map<std::string, size_t> m_counters; // model_name -> counter
    mutable std::mutex m_mutex;
    
public:
    std::string selectInstance(
        const RequestContext& context,
        const std::vector<ModelInstance*>& instances
    ) override {
        if (instances.empty()) return "";
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Group instances by model name to maintain separate counters
        std::string model_name;
        if (!instances.empty()) {
            model_name = instances[0]->model_name;
        }
        
        size_t& counter = m_counters[model_name];
        size_t selected_index = counter % instances.size();
        counter++;
        
        return instances[selected_index]->instance_id;
    }
    
    std::string getName() const override {
        return "round_robin";
    }
};

/**
 * @brief Least-loaded load balancing strategy
 */
class LoadBalancer::LeastLoadedStrategy : public BalancingStrategy {
public:
    std::string selectInstance(
        const RequestContext& context,
        const std::vector<ModelInstance*>& instances
    ) override {
        if (instances.empty()) return "";
        
        // Find instance with minimum active requests
        auto min_it = std::min_element(instances.begin(), instances.end(),
            [](const ModelInstance* a, const ModelInstance* b) {
                return a->active_requests.load() < b->active_requests.load();
            });
        
        return (*min_it)->instance_id;
    }
    
    std::string getName() const override {
        return "least_loaded";
    }
};

/**
 * @brief Response time optimization strategy
 */
class LoadBalancer::ResponseTimeStrategy : public BalancingStrategy {
public:
    std::string selectInstance(
        const RequestContext& context,
        const std::vector<ModelInstance*>& instances
    ) override {
        if (instances.empty()) return "";
        
        // Find instance with best average response time
        auto best_it = std::min_element(instances.begin(), instances.end(),
            [](const ModelInstance* a, const ModelInstance* b) {
                double a_time = a->average_response_time.load();
                double b_time = b->average_response_time.load();
                
                // If one has no history, prefer the one with history
                if (a_time == 0.0 && b_time > 0.0) return false;
                if (b_time == 0.0 && a_time > 0.0) return true;
                if (a_time == 0.0 && b_time == 0.0) {
                    // Both have no history, prefer least loaded
                    return a->active_requests.load() < b->active_requests.load();
                }
                
                return a_time < b_time;
            });
        
        return (*best_it)->instance_id;
    }
    
    std::string getName() const override {
        return "response_time";
    }
};

/**
 * @brief Resource usage balancing strategy
 */
class LoadBalancer::ResourceUsageStrategy : public BalancingStrategy {
public:
    std::string selectInstance(
        const RequestContext& context,
        const std::vector<ModelInstance*>& instances
    ) override {
        if (instances.empty()) return "";
        
        // Find instance with lowest resource usage
        auto best_it = std::min_element(instances.begin(), instances.end(),
            [](const ModelInstance* a, const ModelInstance* b) {
                // Combine memory usage and active requests as resource metric
                double a_load = a->current_memory_usage + (a->active_requests.load() * 0.1);
                double b_load = b->current_memory_usage + (b->active_requests.load() * 0.1);
                return a_load < b_load;
            });
        
        return (*best_it)->instance_id;
    }
    
    std::string getName() const override {
        return "resource_usage";
    }
};

/**
 * @brief Weighted round-robin strategy
 */
class LoadBalancer::WeightedRoundRobinStrategy : public BalancingStrategy {
private:
    std::unordered_map<std::string, size_t> m_counters; // instance_id -> counter
    std::unordered_map<std::string, double> m_weights; // instance_id -> weight
    mutable std::mutex m_mutex;
    
public:
    void setWeights(const std::unordered_map<std::string, double>& weights) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_weights = weights;
    }
    
    std::string selectInstance(
        const RequestContext& context,
        const std::vector<ModelInstance*>& instances
    ) override {
        if (instances.empty()) return "";
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Calculate weighted probabilities
        std::vector<std::pair<ModelInstance*, double>> weighted_instances;
        double total_weight = 0.0;
        
        for (auto* instance : instances) {
            double weight = 1.0; // Default weight
            auto weight_it = m_weights.find(instance->instance_id);
            if (weight_it != m_weights.end()) {
                weight = weight_it->second;
            }
            
            weighted_instances.push_back({instance, weight});
            total_weight += weight;
        }
        
        if (total_weight == 0.0) {
            // Fallback to round-robin if no weights
            size_t& counter = m_counters["fallback"];
            size_t selected_index = counter % instances.size();
            counter++;
            return instances[selected_index]->instance_id;
        }
        
        // Select based on weighted probability
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, total_weight);
        
        double random_value = dis(gen);
        double cumulative_weight = 0.0;
        
        for (const auto& [instance, weight] : weighted_instances) {
            cumulative_weight += weight;
            if (random_value <= cumulative_weight) {
                return instance->instance_id;
            }
        }
        
        // Fallback to last instance
        return weighted_instances.back().first->instance_id;
    }
    
    std::string getName() const override {
        return "weighted_round_robin";
    }
};

// =================================================================
// LoadBalancer Implementation
// =================================================================

LoadBalancer::LoadBalancer(ModelRegistry& registry, const LoadBalancerConfig& config)
    : m_registry(registry), m_config(config), m_current_strategy(config.default_strategy) {
    
    // Register built-in strategies
    registerStrategy(LoadBalancingStrategy::ROUND_ROBIN, 
                    std::make_shared<RoundRobinStrategy>());
    registerStrategy(LoadBalancingStrategy::LEAST_LOADED, 
                    std::make_shared<LeastLoadedStrategy>());
    registerStrategy(LoadBalancingStrategy::RESPONSE_TIME, 
                    std::make_shared<ResponseTimeStrategy>());
    registerStrategy(LoadBalancingStrategy::RESOURCE_USAGE, 
                    std::make_shared<ResourceUsageStrategy>());
    registerStrategy(LoadBalancingStrategy::WEIGHTED_ROUND_ROBIN, 
                    std::make_shared<WeightedRoundRobinStrategy>());
    
    // Start health check thread
    if (m_config.health_check_interval.count() > 0) {
        startHealthCheckThread();
    }
    
    Logger::getInstance().info("LoadBalancer", "Initialized with strategy: " + 
                              std::to_string(static_cast<int>(m_current_strategy)));
}

LoadBalancer::~LoadBalancer() {
    stopHealthCheckThread();
}

LoadBalancingResult LoadBalancer::selectInstance(const std::string& model_name, const RequestContext& context) {
    auto start_time = std::chrono::steady_clock::now();
    LoadBalancingResult result;
    
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    // Get instances for the model
    auto instances = getInstancesForModel(model_name);
    
    // Filter out unhealthy instances
    std::vector<ModelInstance*> healthy_instances;
    for (auto* instance : instances) {
        if (instance->is_healthy.load() && 
            instance->active_requests.load() < m_config.max_requests_per_instance) {
            healthy_instances.push_back(instance);
        }
    }
    
    if (healthy_instances.empty()) {
        // Try to create new instance if auto-scaling is enabled
        if (m_config.auto_scale && instances.size() < m_config.max_instances_per_model) {
            std::string new_instance_id = createInstance(model_name);
            if (!new_instance_id.empty()) {
                auto* new_instance = getInstance(new_instance_id);
                if (new_instance && new_instance->is_healthy.load()) {
                    healthy_instances.push_back(new_instance);
                }
            }
        }
        
        if (healthy_instances.empty()) {
            result.selection_reason = "No healthy instances available for model: " + model_name;
            Logger::getInstance().error("LoadBalancer", result.selection_reason);
            return result;
        }
    }
    
    // Use current strategy to select instance
    auto strategy_it = m_strategies.find(m_current_strategy);
    if (strategy_it == m_strategies.end()) {
        result.selection_reason = "Load balancing strategy not found";
        Logger::getInstance().error("LoadBalancer", result.selection_reason);
        return result;
    }
    
    std::string selected_id = strategy_it->second->selectInstance(context, healthy_instances);
    if (selected_id.empty()) {
        result.selection_reason = "Strategy failed to select instance";
        Logger::getInstance().error("LoadBalancer", result.selection_reason);
        return result;
    }
    
    auto* selected_instance = getInstance(selected_id);
    if (!selected_instance) {
        result.selection_reason = "Selected instance not found: " + selected_id;
        Logger::getInstance().error("LoadBalancer", result.selection_reason);
        return result;
    }
    
    // Build result
    result.selected_instance_id = selected_id;
    result.model = selected_instance->model;
    result.selection_reason = "Selected by " + strategy_it->second->getName() + " strategy";
    
    // Add alternatives
    for (auto* instance : healthy_instances) {
        if (instance->instance_id != selected_id) {
            result.alternative_instances.push_back(instance->instance_id);
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.selection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    Logger::getInstance().debug("LoadBalancer", 
        "Selected instance " + selected_id + " for model " + model_name);
    
    return result;
}

std::string LoadBalancer::createInstance(const std::string& model_name) {
    // Get model from registry
    auto model = m_registry.getModel(model_name);
    if (!model) {
        Logger::getInstance().error("LoadBalancer", "Model not found in registry: " + model_name);
        return "";
    }
    
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    // Check if we've reached the limit
    auto& model_instances = m_model_instances[model_name];
    if (model_instances.size() >= m_config.max_instances_per_model) {
        Logger::getInstance().warning("LoadBalancer", 
            "Maximum instances reached for model: " + model_name);
        return "";
    }
    
    // Generate unique instance ID
    std::string instance_id = generateInstanceId(model_name);
    
    // Create instance
    auto instance = std::make_unique<ModelInstance>();
    instance->instance_id = instance_id;
    instance->model_name = model_name;
    instance->model = model;
    instance->is_healthy.store(true);
    instance->active_requests.store(0);
    instance->average_response_time.store(0.0);
    instance->total_requests.store(0);
    instance->failed_requests.store(0);
    instance->created_at = std::chrono::system_clock::now();
    instance->last_used = std::chrono::system_clock::now();
    instance->last_health_check = std::chrono::system_clock::now();
    
    // Get model configuration for memory estimates
    auto model_configs = m_registry.getConfiguredModels();
    for (const auto& config : model_configs) {
        if (config.name == model_name) {
            // Parse memory usage string (e.g., "8GB")
            std::string memory_str = config.memory_usage;
            if (!memory_str.empty()) {
                try {
                    size_t pos = memory_str.find("GB");
                    if (pos != std::string::npos) {
                        double memory_gb = std::stod(memory_str.substr(0, pos));
                        instance->max_memory_usage = memory_gb;
                        instance->current_memory_usage = memory_gb * 0.8; // Estimate 80% usage
                    }
                } catch (const std::exception&) {
                    instance->max_memory_usage = 4.0; // Default 4GB
                    instance->current_memory_usage = 3.2;
                }
            }
            break;
        }
    }
    
    // Store instance
    model_instances.push_back(instance_id);
    m_instances[instance_id] = std::move(instance);
    
    Logger::getInstance().info("LoadBalancer", 
        "Created instance " + instance_id + " for model " + model_name);
    
    return instance_id;
}

bool LoadBalancer::removeInstance(const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    auto instance_it = m_instances.find(instance_id);
    if (instance_it == m_instances.end()) {
        return false;
    }
    
    std::string model_name = instance_it->second->model_name;
    
    // Remove from model instances list
    auto& model_instances = m_model_instances[model_name];
    model_instances.erase(
        std::remove(model_instances.begin(), model_instances.end(), instance_id),
        model_instances.end());
    
    // Remove instance
    m_instances.erase(instance_it);
    
    Logger::getInstance().info("LoadBalancer", 
        "Removed instance " + instance_id + " for model " + model_name);
    
    return true;
}

std::vector<ModelInstance*> LoadBalancer::getInstancesForModel(const std::string& model_name) {
    std::vector<ModelInstance*> instances;
    
    auto model_it = m_model_instances.find(model_name);
    if (model_it != m_model_instances.end()) {
        for (const auto& instance_id : model_it->second) {
            auto instance_it = m_instances.find(instance_id);
            if (instance_it != m_instances.end()) {
                instances.push_back(instance_it->second.get());
            }
        }
    }
    
    return instances;
}

ModelInstance* LoadBalancer::getInstance(const std::string& instance_id) {
    auto it = m_instances.find(instance_id);
    return (it != m_instances.end()) ? it->second.get() : nullptr;
}

void LoadBalancer::registerStrategy(LoadBalancingStrategy strategy, 
                                  std::shared_ptr<BalancingStrategy> implementation) {
    m_strategies[strategy] = implementation;
    Logger::getInstance().info("LoadBalancer", 
        "Registered strategy: " + implementation->getName());
}

void LoadBalancer::setStrategy(LoadBalancingStrategy strategy) {
    if (m_strategies.find(strategy) != m_strategies.end()) {
        m_current_strategy = strategy;
        Logger::getInstance().info("LoadBalancer", 
            "Active strategy set to: " + std::to_string(static_cast<int>(strategy)));
    } else {
        Logger::getInstance().warning("LoadBalancer", 
            "Strategy not found: " + std::to_string(static_cast<int>(strategy)));
    }
}

LoadBalancingStrategy LoadBalancer::getStrategy() const {
    return m_current_strategy;
}

void LoadBalancer::recordRequestStart(const std::string& instance_id, const std::string& request_id) {
    auto* instance = getInstance(instance_id);
    if (instance) {
        instance->active_requests.fetch_add(1);
        instance->last_used = std::chrono::system_clock::now();
        
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        m_active_requests[request_id] = instance_id;
        
        Logger::getInstance().debug("LoadBalancer", 
            "Started request " + request_id + " on instance " + instance_id);
    }
}

void LoadBalancer::recordRequestEnd(const std::string& instance_id, const std::string& request_id,
                                  double response_time, bool success) {
    auto* instance = getInstance(instance_id);
    if (instance) {
        // Update active requests
        if (instance->active_requests.load() > 0) {
            instance->active_requests.fetch_sub(1);
        }
        
        // Update statistics
        instance->total_requests.fetch_add(1);
        if (!success) {
            instance->failed_requests.fetch_add(1);
        }
        
        // Update average response time (exponential moving average)
        double current_avg = instance->average_response_time.load();
        if (current_avg == 0.0) {
            instance->average_response_time.store(response_time);
        } else {
            double alpha = 0.2; // Smoothing factor
            double new_avg = alpha * response_time + (1.0 - alpha) * current_avg;
            instance->average_response_time.store(new_avg);
        }
        
        // Remove from active requests
        {
            std::lock_guard<std::mutex> lock(m_requests_mutex);
            m_active_requests.erase(request_id);
        }
        
        // Update strategy state
        auto strategy_it = m_strategies.find(m_current_strategy);
        if (strategy_it != m_strategies.end()) {
            strategy_it->second->updateState(instance_id, response_time, success);
        }
        
        Logger::getInstance().debug("LoadBalancer", 
            "Completed request " + request_id + " on instance " + instance_id + 
            " in " + std::to_string(response_time) + "ms");
    }
}

size_t LoadBalancer::performHealthChecks() {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    size_t healthy_count = 0;
    auto now = std::chrono::system_clock::now();
    
    for (auto& [instance_id, instance] : m_instances) {
        // Simple health check - verify model is still available
        bool is_healthy = (instance->model != nullptr);
        
        // Check if instance has too many failures
        if (instance->total_requests.load() > 10) {
            double failure_rate = static_cast<double>(instance->failed_requests.load()) / 
                                 instance->total_requests.load();
            if (failure_rate > 0.5) { // More than 50% failure rate
                is_healthy = false;
            }
        }
        
        // Check response time threshold
        if (instance->average_response_time.load() > m_config.response_time_threshold) {
            is_healthy = false;
        }
        
        instance->is_healthy.store(is_healthy);
        instance->last_health_check = now;
        
        if (is_healthy) {
            healthy_count++;
        }
    }
    
    Logger::getInstance().debug("LoadBalancer", 
        "Health check complete: " + std::to_string(healthy_count) + 
        "/" + std::to_string(m_instances.size()) + " instances healthy");
    
    return healthy_count;
}

std::string LoadBalancer::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    std::ostringstream stats;
    stats << "Load Balancer Statistics\n";
    stats << "========================\n\n";
    
    stats << "Strategy: " << static_cast<int>(m_current_strategy) << "\n";
    stats << "Total Instances: " << m_instances.size() << "\n";
    stats << "Healthy Instances: " << getHealthyInstances() << "\n\n";
    
    // Per-model statistics
    for (const auto& [model_name, instance_ids] : m_model_instances) {
        stats << "Model: " << model_name << "\n";
        stats << "  Instances: " << instance_ids.size() << "\n";
        
        size_t total_requests = 0;
        size_t active_requests = 0;
        double avg_response_time = 0.0;
        size_t healthy_instances = 0;
        
        for (const auto& instance_id : instance_ids) {
            auto instance_it = m_instances.find(instance_id);
            if (instance_it != m_instances.end()) {
                const auto& instance = instance_it->second;
                total_requests += instance->total_requests.load();
                active_requests += instance->active_requests.load();
                avg_response_time += instance->average_response_time.load();
                if (instance->is_healthy.load()) healthy_instances++;
            }
        }
        
        if (!instance_ids.empty()) {
            avg_response_time /= instance_ids.size();
        }
        
        stats << "  Healthy: " << healthy_instances << "/" << instance_ids.size() << "\n";
        stats << "  Total Requests: " << total_requests << "\n";
        stats << "  Active Requests: " << active_requests << "\n";
        stats << "  Avg Response Time: " << std::fixed << std::setprecision(1) 
               << avg_response_time << "ms\n\n";
    }
    
    return stats.str();
}

size_t LoadBalancer::autoScale(const std::string& model_name) {
    if (!m_config.auto_scale) {
        return getInstancesForModel(model_name).size();
    }
    
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    auto instances = getInstancesForModel(model_name);
    
    // Check if scaling is needed
    bool scale_up = false;
    bool scale_down = false;
    
    size_t total_active = 0;
    size_t healthy_count = 0;
    double avg_response_time = 0.0;
    
    for (auto* instance : instances) {
        total_active += instance->active_requests.load();
        if (instance->is_healthy.load()) {
            healthy_count++;
            avg_response_time += instance->average_response_time.load();
        }
    }
    
    if (healthy_count > 0) {
        avg_response_time /= healthy_count;
    }
    
    // Scale up conditions
    if (instances.size() < m_config.max_instances_per_model) {
        if (total_active > instances.size() * m_config.max_requests_per_instance * 0.8 ||
            avg_response_time > m_config.response_time_threshold * 0.8) {
            scale_up = true;
        }
    }
    
    // Scale down conditions  
    if (instances.size() > 1) {
        if (total_active < instances.size() * m_config.max_requests_per_instance * 0.2 &&
            avg_response_time < m_config.response_time_threshold * 0.5) {
            scale_down = true;
        }
    }
    
    if (scale_up) {
        std::string new_instance = createInstance(model_name);
        if (!new_instance.empty()) {
            Logger::getInstance().info("LoadBalancer", 
                "Scaled up model " + model_name + " (new instance: " + new_instance + ")");
        }
    } else if (scale_down) {
        // Remove least used instance
        auto least_used_it = std::min_element(instances.begin(), instances.end(),
            [](const ModelInstance* a, const ModelInstance* b) {
                return a->last_used < b->last_used;
            });
        
        if (least_used_it != instances.end() && (*least_used_it)->active_requests.load() == 0) {
            std::string instance_id = (*least_used_it)->instance_id;
            removeInstance(instance_id);
            Logger::getInstance().info("LoadBalancer", 
                "Scaled down model " + model_name + " (removed instance: " + instance_id + ")");
        }
    }
    
    return getInstancesForModel(model_name).size();
}

size_t LoadBalancer::cleanupInactiveInstances() {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    auto now = std::chrono::system_clock::now();
    size_t removed_count = 0;
    
    std::vector<std::string> to_remove;
    
    for (const auto& [instance_id, instance] : m_instances) {
        auto inactive_time = std::chrono::duration_cast<std::chrono::seconds>(
            now - instance->last_used);
        
        if (inactive_time > m_config.instance_timeout && 
            instance->active_requests.load() == 0) {
            to_remove.push_back(instance_id);
        }
    }
    
    for (const auto& instance_id : to_remove) {
        if (removeInstance(instance_id)) {
            removed_count++;
        }
    }
    
    if (removed_count > 0) {
        Logger::getInstance().info("LoadBalancer", 
            "Cleaned up " + std::to_string(removed_count) + " inactive instances");
    }
    
    return removed_count;
}

LoadBalancerConfig LoadBalancer::getConfig() const {
    return m_config;
}

void LoadBalancer::setConfig(const LoadBalancerConfig& config) {
    m_config = config;
    setStrategy(config.default_strategy);
    Logger::getInstance().info("LoadBalancer", "Configuration updated");
}

void LoadBalancer::setAutoScaling(bool enabled) {
    m_config.auto_scale = enabled;
    Logger::getInstance().info("LoadBalancer", 
        "Auto-scaling " + std::string(enabled ? "enabled" : "disabled"));
}

size_t LoadBalancer::getTotalInstances() const {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    return m_instances.size();
}

size_t LoadBalancer::getHealthyInstances() const {
    std::lock_guard<std::mutex> lock(m_instances_mutex);
    
    size_t healthy_count = 0;
    for (const auto& [instance_id, instance] : m_instances) {
        if (instance->is_healthy.load()) {
            healthy_count++;
        }
    }
    
    return healthy_count;
}

std::string LoadBalancer::generateInstanceId(const std::string& model_name) {
    size_t id = m_next_instance_id.fetch_add(1);
    return model_name + "_instance_" + std::to_string(id);
}

bool LoadBalancer::needsScaling(const std::string& model_name) {
    auto instances = getInstancesForModel(model_name);
    
    if (instances.empty()) {
        return true; // Need at least one instance
    }
    
    // Check load
    size_t total_active = 0;
    for (auto* instance : instances) {
        total_active += instance->active_requests.load();
    }
    
    double load_ratio = static_cast<double>(total_active) / 
                       (instances.size() * m_config.max_requests_per_instance);
    
    return load_ratio > 0.8; // Scale if more than 80% loaded
}

void LoadBalancer::startHealthCheckThread() {
    m_stop_health_checks.store(false);
    m_health_check_thread = std::make_unique<std::thread>(&LoadBalancer::healthCheckLoop, this);
    Logger::getInstance().info("LoadBalancer", "Started health check thread");
}

void LoadBalancer::stopHealthCheckThread() {
    if (m_health_check_thread) {
        m_stop_health_checks.store(true);
        m_health_check_thread->join();
        m_health_check_thread.reset();
        Logger::getInstance().info("LoadBalancer", "Stopped health check thread");
    }
}

void LoadBalancer::healthCheckLoop() {
    while (!m_stop_health_checks.load()) {
        try {
            performHealthChecks();
            
            // Cleanup inactive instances
            cleanupInactiveInstances();
            
            // Auto-scale models if needed
            if (m_config.auto_scale) {
                std::vector<std::string> model_names;
                {
                    std::lock_guard<std::mutex> lock(m_instances_mutex);
                    for (const auto& [model_name, _] : m_model_instances) {
                        model_names.push_back(model_name);
                    }
                }
                
                for (const auto& model_name : model_names) {
                    autoScale(model_name);
                }
            }
        } catch (const std::exception& e) {
            Logger::getInstance().error("LoadBalancer", 
                "Health check loop error: " + std::string(e.what()));
        }
        
        // Sleep for the configured interval
        std::this_thread::sleep_for(m_config.health_check_interval);
    }
}

} // namespace Camus