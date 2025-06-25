// =================================================================
// src/Camus/ParallelStrategy.cpp
// =================================================================
// Implementation of parallel strategy for concurrent model execution.

#include "Camus/ParallelStrategy.hpp"
#include "Camus/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <numeric>
#include <queue>
#include <set>
#include <thread>
#include <condition_variable>
#include <functional>

namespace Camus {

// Thread pool implementation for parallel execution
class ParallelStrategy::ThreadPool {
public:
    ThreadPool(size_t num_threads) : m_stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            m_workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_queue_mutex);
                        m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                        
                        if (m_stop && m_tasks.empty()) {
                            return;
                        }
                        
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_stop = true;
        }
        
        m_condition.notify_all();
        
        for (std::thread& worker : m_workers) {
            worker.join();
        }
    }
    
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            
            if (m_stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            m_tasks.emplace([task]() { (*task)(); });
        }
        
        m_condition.notify_one();
        return res;
    }
    
    size_t queueSize() const {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        return m_tasks.size();
    }
    
private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_condition;
    bool m_stop;
};

ParallelStrategy::ParallelStrategy(ModelRegistry& registry, 
                                 const ParallelStrategyConfig& config)
    : m_registry(registry), m_config(config) {
    
    // Initialize thread pool
    initializeThreadPool();
    
    // Initialize default templates and aggregators
    initializeDefaultTemplates();
    initializeDefaultAggregators();
    
    // Initialize statistics
    m_statistics.last_reset = std::chrono::system_clock::now();
    
    // Start resource monitoring thread
    if (m_config.enable_resource_monitoring) {
        std::thread monitor_thread(&ParallelStrategy::monitorResources, this);
        monitor_thread.detach();
    }
    
    Logger::getInstance().info("ParallelStrategy", "Strategy initialized successfully");
}

ParallelStrategy::~ParallelStrategy() {
    shutdown();
}

ParallelResponse ParallelStrategy::execute(const ParallelRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    ParallelResponse response;
    response.request_id = request.request_id;
    response.pattern_used = request.pattern;
    response.success = false;
    
    Logger::getInstance().debug("ParallelStrategy", 
        "Executing parallel request: " + request.request_id + 
        " with pattern: " + parallelPatternToString(request.pattern));
    
    try {
        // Get subtasks based on pattern or use provided subtasks
        std::vector<ParallelSubtask> subtasks;
        if (!request.subtasks.empty()) {
            subtasks = request.subtasks;
        } else {
            subtasks = getSubtasksForPattern(request.pattern, request);
        }
        
        if (subtasks.empty()) {
            throw std::runtime_error("No subtasks defined for parallel execution");
        }
        
        response.total_subtasks = subtasks.size();
        
        // Wait for resources if needed
        if (request.enable_resource_limits) {
            if (!waitForResources(request.timeout / 2)) {
                throw std::runtime_error("Resource limits exceeded, unable to execute");
            }
        }
        
        // Execute subtasks in parallel
        auto parallel_start = std::chrono::steady_clock::now();
        std::vector<SubtaskResult> results = executeSubtasks(request, subtasks);
        auto parallel_end = std::chrono::steady_clock::now();
        
        response.parallel_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            parallel_end - parallel_start);
        
        // Count successful subtasks
        for (const auto& result : results) {
            if (result.success) {
                response.subtasks_succeeded++;
            }
            response.subtasks_completed++;
        }
        
        response.subtask_results = results;
        
        // Check minimum success ratio
        double success_ratio = static_cast<double>(response.subtasks_succeeded) / 
                              response.total_subtasks;
        if (success_ratio < request.min_success_ratio) {
            throw std::runtime_error("Insufficient successful subtasks: " + 
                                   std::to_string(response.subtasks_succeeded) + "/" + 
                                   std::to_string(response.total_subtasks));
        }
        
        // Resolve conflicts if needed
        auto aggregation_start = std::chrono::steady_clock::now();
        
        std::vector<SubtaskResult> resolved_results = results;
        if (results.size() > 1) {
            resolved_results = resolveConflicts(results, request.conflict_strategy, 
                                              response.conflict_notes);
        }
        
        // Aggregate results
        response.aggregated_result = aggregateResults(resolved_results, 
                                                    request.aggregation_method, 
                                                    request);
        
        auto aggregation_end = std::chrono::steady_clock::now();
        response.aggregation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            aggregation_end - aggregation_start);
        
        // Calculate quality score
        response.overall_quality_score = calculateAggregatedQuality(
            response.aggregated_result, resolved_results);
        
        response.success = true;
        
        // Calculate speedup factor
        auto sequential_time = std::accumulate(results.begin(), results.end(), 
            std::chrono::milliseconds(0),
            [](const auto& acc, const auto& result) {
                return acc + result.execution_time;
            });
        
        if (sequential_time.count() > 0) {
            response.speedup_factor = static_cast<double>(sequential_time.count()) / 
                                    response.parallel_time.count();
        }
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Parallel execution failed: " + std::string(e.what());
        
        Logger::getInstance().error("ParallelStrategy", 
            "Parallel execution failed: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    response.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    // Update resource usage stats
    response.resource_usage["peak_threads"] = m_active_executions.load();
    response.resource_usage["queue_size"] = m_thread_pool->queueSize();
    
    // Update statistics
    updateStatistics(request, response);
    
    return response;
}

std::vector<SubtaskResult> ParallelStrategy::executeSubtasks(
    const ParallelRequest& request,
    const std::vector<ParallelSubtask>& subtasks) {
    
    std::vector<SubtaskResult> results;
    
    // Resolve dependencies if enabled
    if (m_config.enable_dependency_resolution) {
        // Group subtasks by dependency levels
        auto dependency_groups = resolveDependencies(subtasks);
        
        // Execute each dependency level sequentially, but tasks within level in parallel
        for (const auto& group : dependency_groups) {
            std::vector<std::future<SubtaskResult>> futures;
            
            for (const auto& subtask : group) {
                // Check concurrent execution limit
                while (m_active_executions >= request.max_concurrent_tasks) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                m_active_executions++;
                
                futures.push_back(
                    m_thread_pool->enqueue([this, &subtask, &request]() {
                        auto result = executeSubtask(subtask, request);
                        m_active_executions--;
                        return result;
                    })
                );
            }
            
            // Wait for all tasks in this group to complete
            for (auto& future : futures) {
                try {
                    results.push_back(future.get());
                } catch (const std::exception& e) {
                    SubtaskResult error_result;
                    error_result.subtask_id = "unknown";
                    error_result.success = false;
                    error_result.error_message = e.what();
                    results.push_back(error_result);
                }
            }
        }
    } else {
        // Execute all subtasks in parallel without dependency resolution
        std::vector<std::future<SubtaskResult>> futures;
        
        for (const auto& subtask : subtasks) {
            // Check concurrent execution limit
            while (m_active_executions >= request.max_concurrent_tasks) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            m_active_executions++;
            
            futures.push_back(
                m_thread_pool->enqueue([this, &subtask, &request]() {
                    auto result = executeSubtask(subtask, request);
                    m_active_executions--;
                    return result;
                })
            );
        }
        
        // Collect results
        for (auto& future : futures) {
            try {
                results.push_back(future.get());
            } catch (const std::exception& e) {
                SubtaskResult error_result;
                error_result.subtask_id = "unknown";
                error_result.success = false;
                error_result.error_message = e.what();
                results.push_back(error_result);
            }
        }
    }
    
    return results;
}

SubtaskResult ParallelStrategy::executeSubtask(const ParallelSubtask& subtask,
                                              const ParallelRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    SubtaskResult result;
    result.subtask_id = subtask.subtask_id;
    result.model_used = subtask.model_name;
    
    try {
        // Get model from registry
        auto model = m_registry.getModel(subtask.model_name);
        if (!model) {
            throw std::runtime_error("Model not available: " + subtask.model_name);
        }
        
        // Execute model with timeout
        std::future<std::string> future = std::async(std::launch::async, 
            [&model, &subtask]() {
                return model->getCompletion(subtask.prompt);
            });
        
        if (future.wait_for(subtask.timeout) == std::future_status::timeout) {
            throw std::runtime_error("Subtask execution timed out after " + 
                                   std::to_string(subtask.timeout.count()) + "ms");
        }
        
        result.result_text = future.get();
        
        if (result.result_text.empty()) {
            throw std::runtime_error("Model returned empty response");
        }
        
        result.success = true;
        
        // Calculate quality score (simplified)
        result.quality_score = 0.5 + (result.result_text.length() > 100 ? 0.2 : 0.0) +
                              (result.result_text.find("error") == std::string::npos ? 0.3 : 0.0);
        
        // Add metadata
        result.metadata["model_name"] = subtask.model_name;
        result.metadata["weight"] = std::to_string(subtask.weight);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        result.quality_score = 0.0;
        
        // Retry if enabled and not critical
        if (m_config.enable_failure_recovery && !subtask.is_critical) {
            for (size_t attempt = 1; attempt <= m_config.max_retry_attempts; ++attempt) {
                Logger::getInstance().warn("ParallelStrategy", 
                    "Retrying subtask '" + subtask.subtask_id + "' attempt " + 
                    std::to_string(attempt));
                
                try {
                    auto model = m_registry.getModel(subtask.model_name);
                    if (model) {
                        result.result_text = model->getCompletion(subtask.prompt);
                        result.success = true;
                        result.error_message.clear();
                        result.quality_score = 0.5;
                        break;
                    }
                } catch (...) {
                    // Continue to next retry
                }
            }
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    return result;
}

std::string ParallelStrategy::aggregateResults(const std::vector<SubtaskResult>& results,
                                              AggregationMethod method,
                                              const ParallelRequest& request) {
    // Filter successful results
    std::vector<SubtaskResult> successful_results;
    for (const auto& result : results) {
        if (result.success && !result.result_text.empty()) {
            successful_results.push_back(result);
        }
    }
    
    if (successful_results.empty()) {
        return "No successful results to aggregate";
    }
    
    // Use custom aggregator if provided
    if (method == AggregationMethod::CUSTOM && request.custom_aggregator) {
        return request.custom_aggregator(successful_results);
    }
    
    // Check for registered custom aggregator
    auto it = m_config.aggregation_functions.find(method);
    if (it != m_config.aggregation_functions.end() && it->second) {
        return it->second(successful_results);
    }
    
    // Use default aggregators
    switch (method) {
        case AggregationMethod::CONCATENATE:
            return concatenateAggregator(successful_results);
            
        case AggregationMethod::MERGE:
            return mergeAggregator(successful_results);
            
        case AggregationMethod::BEST_QUALITY:
            return bestQualityAggregator(successful_results);
            
        case AggregationMethod::CONSENSUS:
            return consensusAggregator(successful_results);
            
        case AggregationMethod::WEIGHTED_COMBINE:
            return weightedCombineAggregator(successful_results);
            
        default:
            return mergeAggregator(successful_results);
    }
}

std::vector<SubtaskResult> ParallelStrategy::resolveConflicts(
    const std::vector<SubtaskResult>& results,
    ConflictResolution strategy,
    std::vector<std::string>& conflicts) {
    
    std::vector<SubtaskResult> resolved_results;
    
    // Group results by aggregation group if specified
    std::unordered_map<std::string, std::vector<SubtaskResult>> grouped_results;
    for (const auto& result : results) {
        if (result.success) {
            std::string group = result.metadata.count("aggregation_group") > 0 ?
                               result.metadata.at("aggregation_group") : "default";
            grouped_results[group].push_back(result);
        }
    }
    
    // Resolve conflicts within each group
    for (const auto& [group, group_results] : grouped_results) {
        if (group_results.size() == 1) {
            resolved_results.push_back(group_results[0]);
            continue;
        }
        
        switch (strategy) {
            case ConflictResolution::MAJORITY_VOTE: {
                // Find most common result pattern
                std::unordered_map<std::string, std::vector<SubtaskResult>> pattern_map;
                for (const auto& result : group_results) {
                    // Simple pattern: first 100 chars
                    std::string pattern = result.result_text.substr(0, 100);
                    pattern_map[pattern].push_back(result);
                }
                
                // Find majority
                size_t max_count = 0;
                std::vector<SubtaskResult> majority_results;
                for (const auto& [pattern, pattern_results] : pattern_map) {
                    if (pattern_results.size() > max_count) {
                        max_count = pattern_results.size();
                        majority_results = pattern_results;
                    }
                }
                
                if (majority_results.size() > group_results.size() / 2) {
                    resolved_results.push_back(majority_results[0]);
                    conflicts.push_back("Resolved conflict in group '" + group + 
                                      "' using majority vote");
                } else {
                    // No clear majority, use highest quality
                    auto best_it = std::max_element(group_results.begin(), group_results.end(),
                        [](const auto& a, const auto& b) {
                            return a.quality_score < b.quality_score;
                        });
                    resolved_results.push_back(*best_it);
                    conflicts.push_back("No majority in group '" + group + 
                                      "', used highest quality result");
                }
                break;
            }
            
            case ConflictResolution::HIGHEST_CONFIDENCE: {
                auto best_it = std::max_element(group_results.begin(), group_results.end(),
                    [](const auto& a, const auto& b) {
                        return a.quality_score < b.quality_score;
                    });
                resolved_results.push_back(*best_it);
                conflicts.push_back("Resolved conflict in group '" + group + 
                                  "' using highest confidence");
                break;
            }
            
            case ConflictResolution::MERGE_ALL: {
                // Create merged result
                SubtaskResult merged;
                merged.subtask_id = group + "_merged";
                merged.model_used = "multiple";
                merged.success = true;
                
                std::stringstream ss;
                ss << "Merged results from " << group_results.size() << " models:\n\n";
                
                for (size_t i = 0; i < group_results.size(); ++i) {
                    ss << "--- Model " << (i + 1) << " (" << group_results[i].model_used << ") ---\n";
                    ss << group_results[i].result_text << "\n\n";
                }
                
                merged.result_text = ss.str();
                merged.quality_score = std::accumulate(group_results.begin(), group_results.end(), 
                    0.0, [](double sum, const auto& r) { return sum + r.quality_score; }) / 
                    group_results.size();
                
                resolved_results.push_back(merged);
                conflicts.push_back("Merged all results in group '" + group + "'");
                break;
            }
            
            case ConflictResolution::FIRST_VALID: {
                resolved_results.push_back(group_results[0]);
                conflicts.push_back("Used first valid result in group '" + group + "'");
                break;
            }
            
            case ConflictResolution::MANUAL_REVIEW:
            default: {
                // Flag for manual review but include all results
                for (const auto& result : group_results) {
                    resolved_results.push_back(result);
                }
                conflicts.push_back("Group '" + group + 
                                  "' flagged for manual review - multiple perspectives included");
                break;
            }
        }
    }
    
    return resolved_results;
}

std::vector<ParallelSubtask> ParallelStrategy::getSubtasksForPattern(
    ParallelPattern pattern, const ParallelRequest& request) {
    
    // Check for pre-configured pattern templates
    auto it = m_config.pattern_templates.find(pattern);
    if (it != m_config.pattern_templates.end()) {
        return it->second;
    }
    
    // Generate default subtasks based on pattern
    std::vector<ParallelSubtask> subtasks;
    
    switch (pattern) {
        case ParallelPattern::FILE_ANALYSIS: {
            // Extract file paths from context or prompt
            std::vector<std::string> files;
            // Simple extraction - in real implementation would parse more carefully
            std::regex file_regex(R"([\w/\\.-]+\.(cpp|hpp|cc|hh|c|h|py|js|java))");
            std::smatch matches;
            std::string search_text = request.prompt + " " + request.context;
            
            while (std::regex_search(search_text, matches, file_regex)) {
                files.push_back(matches[0]);
                search_text = matches.suffix().str();
            }
            
            if (files.empty()) {
                // Default to analyzing current file
                files.push_back("current_file");
            }
            
            for (const auto& file : files) {
                ParallelSubtask subtask;
                subtask.subtask_id = "analyze_" + file;
                subtask.model_name = "llama3-local";
                subtask.description = "Analyze file: " + file;
                subtask.prompt = "Analyze the following file and provide insights: " + file + 
                               "\n\nContext: " + request.prompt;
                subtask.weight = 1.0;
                subtask.aggregation_group = "file_analysis";
                subtasks.push_back(subtask);
            }
            break;
        }
        
        case ParallelPattern::MULTI_ASPECT_REVIEW: {
            // Security review
            ParallelSubtask security_task;
            security_task.subtask_id = "security_review";
            security_task.model_name = "llama3-local";
            security_task.description = "Security analysis";
            security_task.prompt = "Review the following for security vulnerabilities:\n\n" + 
                                 request.prompt;
            security_task.weight = 1.5;
            security_task.is_critical = true;
            security_task.aggregation_group = "review";
            subtasks.push_back(security_task);
            
            // Performance review
            ParallelSubtask performance_task;
            performance_task.subtask_id = "performance_review";
            performance_task.model_name = "deepseek-coder-local";
            performance_task.description = "Performance analysis";
            performance_task.prompt = "Analyze performance characteristics and optimization opportunities:\n\n" + 
                                    request.prompt;
            performance_task.weight = 1.0;
            performance_task.aggregation_group = "review";
            subtasks.push_back(performance_task);
            
            // Style review
            ParallelSubtask style_task;
            style_task.subtask_id = "style_review";
            style_task.model_name = "deepseek-coder-local";
            style_task.description = "Code style analysis";
            style_task.prompt = "Review code style and suggest improvements:\n\n" + request.prompt;
            style_task.weight = 0.8;
            style_task.aggregation_group = "review";
            subtasks.push_back(style_task);
            break;
        }
        
        case ParallelPattern::ALTERNATIVE_GENERATION: {
            // Generate multiple alternative solutions
            std::vector<std::string> approaches = {
                "object-oriented approach",
                "functional programming approach",
                "performance-optimized approach",
                "memory-efficient approach"
            };
            
            for (size_t i = 0; i < approaches.size(); ++i) {
                ParallelSubtask subtask;
                subtask.subtask_id = "alternative_" + std::to_string(i);
                subtask.model_name = (i % 2 == 0) ? "llama3-local" : "deepseek-coder-local";
                subtask.description = "Generate " + approaches[i];
                subtask.prompt = "Provide a solution using " + approaches[i] + ":\n\n" + 
                               request.prompt;
                subtask.weight = 1.0;
                subtask.aggregation_group = "alternatives";
                subtasks.push_back(subtask);
            }
            break;
        }
        
        case ParallelPattern::VALIDATION_SUITE: {
            // Multiple validation checks
            std::vector<std::pair<std::string, std::string>> validations = {
                {"syntax_validation", "Validate syntax and structure"},
                {"logic_validation", "Validate logic and correctness"},
                {"edge_case_validation", "Check edge cases and error handling"},
                {"integration_validation", "Validate integration points"}
            };
            
            for (const auto& [id, description] : validations) {
                ParallelSubtask subtask;
                subtask.subtask_id = id;
                subtask.model_name = "llama3-local";
                subtask.description = description;
                subtask.prompt = description + " for:\n\n" + request.prompt;
                subtask.weight = 1.0;
                subtask.is_critical = (id == "syntax_validation");
                subtask.aggregation_group = "validation";
                subtasks.push_back(subtask);
            }
            break;
        }
        
        case ParallelPattern::CUSTOM:
        default:
            // Return empty vector for custom pattern
            break;
    }
    
    return subtasks;
}

bool ParallelStrategy::checkResourceAvailability() {
    if (!m_config.enable_resource_monitoring) {
        return true;
    }
    
    return m_current_cpu_usage < m_config.max_cpu_usage_percent &&
           m_current_memory_usage < m_config.max_memory_usage_percent &&
           m_active_executions < m_config.max_concurrent_executions;
}

bool ParallelStrategy::waitForResources(std::chrono::milliseconds timeout) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (!checkResourceAvailability()) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Adaptive throttling - reduce concurrent executions if resources are constrained
        if (m_config.enable_adaptive_throttling) {
            if (m_current_cpu_usage > m_config.max_cpu_usage_percent * 0.9 ||
                m_current_memory_usage > m_config.max_memory_usage_percent * 0.9) {
                
                size_t new_limit = std::max(size_t(1), 
                    m_config.max_concurrent_executions * 3 / 4);
                
                Logger::getInstance().warn("ParallelStrategy", 
                    "Throttling concurrent executions to " + std::to_string(new_limit));
                
                m_config.max_concurrent_executions = new_limit;
            }
        }
    }
    
    return true;
}

void ParallelStrategy::updateResourceUsage(size_t subtask_count) {
    // Simple resource estimation - in real implementation would use system APIs
    m_current_cpu_usage = std::min(100.0, 
        20.0 + (subtask_count * 15.0)); // Base + per-task estimate
    
    m_current_memory_usage = std::min(100.0, 
        30.0 + (subtask_count * 10.0)); // Base + per-task estimate
}

double ParallelStrategy::calculateAggregatedQuality(const std::string& aggregated_result,
                                                  const std::vector<SubtaskResult>& subtask_results) {
    if (subtask_results.empty()) {
        return 0.0;
    }
    
    // Base quality from individual results
    double total_quality = 0.0;
    double total_weight = 0.0;
    
    for (const auto& result : subtask_results) {
        if (result.success) {
            double weight = 1.0;
            if (result.metadata.count("weight") > 0) {
                weight = std::stod(result.metadata.at("weight"));
            }
            
            total_quality += result.quality_score * weight;
            total_weight += weight;
        }
    }
    
    double base_quality = total_weight > 0 ? (total_quality / total_weight) : 0.0;
    
    // Adjust for aggregation quality
    double aggregation_bonus = 0.0;
    
    // Bonus for comprehensive results
    if (aggregated_result.length() > 500) {
        aggregation_bonus += 0.1;
    }
    
    // Bonus for structured output
    if (aggregated_result.find("\n") != std::string::npos && 
        aggregated_result.find("---") != std::string::npos) {
        aggregation_bonus += 0.05;
    }
    
    // Bonus for consensus
    if (subtask_results.size() > 1) {
        double quality_variance = 0.0;
        for (const auto& result : subtask_results) {
            if (result.success) {
                quality_variance += std::pow(result.quality_score - base_quality, 2);
            }
        }
        quality_variance /= subtask_results.size();
        
        // Lower variance means better consensus
        if (quality_variance < 0.1) {
            aggregation_bonus += 0.1;
        }
    }
    
    return std::min(1.0, base_quality + aggregation_bonus);
}

void ParallelStrategy::updateStatistics(const ParallelRequest& request,
                                       const ParallelResponse& response) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_statistics.total_requests++;
    
    if (response.success) {
        m_statistics.successful_requests++;
    } else {
        m_statistics.failed_requests++;
    }
    
    // Update averages
    double n = static_cast<double>(m_statistics.total_requests);
    m_statistics.average_speedup_factor = 
        (m_statistics.average_speedup_factor * (n - 1) + response.speedup_factor) / n;
    
    // Calculate parallel efficiency
    double efficiency = response.speedup_factor / response.total_subtasks;
    m_statistics.average_parallel_efficiency = 
        (m_statistics.average_parallel_efficiency * (n - 1) + efficiency) / n;
    
    // Update resource utilization
    double resource_util = m_current_cpu_usage.load() / 100.0;
    m_statistics.average_resource_utilization = 
        (m_statistics.average_resource_utilization * (n - 1) + resource_util) / n;
    
    // Update subtask statistics
    m_statistics.total_subtasks_executed += response.subtasks_completed;
    m_statistics.total_subtasks_succeeded += response.subtasks_succeeded;
    m_statistics.total_conflicts_resolved += response.conflict_notes.size();
    
    // Update pattern usage
    m_statistics.pattern_usage[request.pattern]++;
    
    // Update aggregation usage
    m_statistics.aggregation_usage[request.aggregation_method]++;
    
    // Update model performance
    for (const auto& result : response.subtask_results) {
        if (result.success) {
            auto& perf = m_statistics.model_subtask_performance[result.model_used];
            size_t model_count = 0;
            for (const auto& r : response.subtask_results) {
                if (r.model_used == result.model_used) model_count++;
            }
            perf = (perf * (model_count - 1) + result.quality_score) / model_count;
        }
    }
    
    // Update concurrency statistics
    size_t current_active = m_active_executions.load();
    m_statistics.peak_concurrent_executions = std::max(
        m_statistics.peak_concurrent_executions, 
        static_cast<double>(current_active));
    
    m_statistics.average_concurrent_executions = 
        (m_statistics.average_concurrent_executions * (n - 1) + current_active) / n;
}

ParallelStrategyConfig ParallelStrategy::getConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

void ParallelStrategy::setConfig(const ParallelStrategyConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
    
    // Reinitialize thread pool if size changed
    if (m_thread_pool) {
        initializeThreadPool();
    }
}

ParallelStatistics ParallelStrategy::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_statistics;
}

void ParallelStrategy::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_statistics = ParallelStatistics();
    m_statistics.last_reset = std::chrono::system_clock::now();
}

ResourceUsage ParallelStrategy::getCurrentResourceUsage() const {
    ResourceUsage usage;
    usage.cpu_usage_percent = m_current_cpu_usage.load();
    usage.memory_usage_percent = m_current_memory_usage.load();
    usage.active_threads = m_active_executions.load();
    usage.queued_tasks = m_thread_pool ? m_thread_pool->queueSize() : 0;
    usage.timestamp = std::chrono::system_clock::now();
    return usage;
}

void ParallelStrategy::registerPatternTemplate(ParallelPattern pattern,
                                              const std::vector<ParallelSubtask>& subtasks) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.pattern_templates[pattern] = subtasks;
}

void ParallelStrategy::registerAggregationFunction(
    AggregationMethod method,
    std::function<std::string(const std::vector<SubtaskResult>&)> aggregator) {
    
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.aggregation_functions[method] = aggregator;
}

void ParallelStrategy::shutdown() {
    m_shutdown_requested = true;
    
    // Wait for active executions to complete
    while (m_active_executions > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ParallelStrategy::initializeThreadPool() {
    size_t num_threads = m_config.thread_pool_size;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) {
            num_threads = 4; // Default fallback
        }
    }
    
    m_thread_pool = std::make_unique<ThreadPool>(num_threads);
    
    Logger::getInstance().info("ParallelStrategy", 
        "Initialized thread pool with " + std::to_string(num_threads) + " threads");
}

void ParallelStrategy::initializeDefaultTemplates() {
    // Default templates are generated dynamically in getSubtasksForPattern
    // This allows for more flexible pattern-based subtask generation
}

void ParallelStrategy::initializeDefaultAggregators() {
    // Register default aggregation functions
    m_config.aggregation_functions[AggregationMethod::CONCATENATE] = concatenateAggregator;
    m_config.aggregation_functions[AggregationMethod::MERGE] = mergeAggregator;
    m_config.aggregation_functions[AggregationMethod::BEST_QUALITY] = bestQualityAggregator;
    m_config.aggregation_functions[AggregationMethod::CONSENSUS] = consensusAggregator;
    m_config.aggregation_functions[AggregationMethod::WEIGHTED_COMBINE] = weightedCombineAggregator;
}

void ParallelStrategy::monitorResources() {
    while (!m_shutdown_requested) {
        updateResourceUsage(m_active_executions.load());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::vector<std::vector<ParallelSubtask>> ParallelStrategy::resolveDependencies(
    const std::vector<ParallelSubtask>& subtasks) {
    
    // Build dependency graph
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    std::unordered_map<std::string, size_t> in_degree;
    std::unordered_map<std::string, ParallelSubtask> subtask_map;
    
    for (const auto& subtask : subtasks) {
        subtask_map[subtask.subtask_id] = subtask;
        in_degree[subtask.subtask_id] = subtask.dependencies.size();
        
        for (const auto& dep : subtask.dependencies) {
            dependents[dep].push_back(subtask.subtask_id);
        }
    }
    
    // Topological sort to create dependency levels
    std::vector<std::vector<ParallelSubtask>> levels;
    std::queue<std::string> ready_queue;
    
    // Find initial tasks with no dependencies
    for (const auto& [id, degree] : in_degree) {
        if (degree == 0) {
            ready_queue.push(id);
        }
    }
    
    while (!ready_queue.empty()) {
        std::vector<ParallelSubtask> current_level;
        size_t level_size = ready_queue.size();
        
        for (size_t i = 0; i < level_size; ++i) {
            std::string task_id = ready_queue.front();
            ready_queue.pop();
            
            current_level.push_back(subtask_map[task_id]);
            
            // Update dependents
            for (const auto& dependent : dependents[task_id]) {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    ready_queue.push(dependent);
                }
            }
        }
        
        levels.push_back(current_level);
    }
    
    return levels;
}

// Static aggregator implementations

std::string ParallelStrategy::concatenateAggregator(const std::vector<SubtaskResult>& results) {
    std::stringstream ss;
    
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) ss << "\n\n";
        ss << "=== Result " << (i + 1) << " ===\n";
        ss << results[i].result_text;
    }
    
    return ss.str();
}

std::string ParallelStrategy::mergeAggregator(const std::vector<SubtaskResult>& results) {
    if (results.empty()) return "";
    if (results.size() == 1) return results[0].result_text;
    
    std::stringstream ss;
    ss << "Merged analysis from " << results.size() << " parallel executions:\n\n";
    
    // Group by aggregation group if available
    std::unordered_map<std::string, std::vector<std::string>> grouped;
    
    for (const auto& result : results) {
        std::string group = "general";
        if (result.metadata.count("aggregation_group") > 0) {
            group = result.metadata.at("aggregation_group");
        }
        grouped[group].push_back(result.result_text);
    }
    
    // Merge each group
    for (const auto& [group, texts] : grouped) {
        if (grouped.size() > 1) {
            ss << "## " << group << "\n\n";
        }
        
        // Simple deduplication and merging
        std::set<std::string> unique_lines;
        for (const auto& text : texts) {
            std::istringstream iss(text);
            std::string line;
            while (std::getline(iss, line)) {
                if (!line.empty()) {
                    unique_lines.insert(line);
                }
            }
        }
        
        for (const auto& line : unique_lines) {
            ss << line << "\n";
        }
        ss << "\n";
    }
    
    return ss.str();
}

std::string ParallelStrategy::bestQualityAggregator(const std::vector<SubtaskResult>& results) {
    if (results.empty()) return "";
    
    auto best_it = std::max_element(results.begin(), results.end(),
        [](const auto& a, const auto& b) {
            return a.quality_score < b.quality_score;
        });
    
    return best_it->result_text;
}

std::string ParallelStrategy::consensusAggregator(const std::vector<SubtaskResult>& results) {
    if (results.empty()) return "";
    if (results.size() == 1) return results[0].result_text;
    
    std::stringstream ss;
    ss << "Consensus analysis based on " << results.size() << " perspectives:\n\n";
    
    // Extract common themes (simplified)
    std::unordered_map<std::string, size_t> word_frequency;
    std::set<std::string> stop_words = {"the", "a", "an", "is", "are", "was", "were", 
                                        "in", "on", "at", "to", "for", "of", "and", "or"};
    
    for (const auto& result : results) {
        std::istringstream iss(result.result_text);
        std::string word;
        while (iss >> word) {
            // Simple word extraction
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            word.erase(std::remove_if(word.begin(), word.end(), 
                [](char c) { return !std::isalnum(c); }), word.end());
            
            if (word.length() > 3 && stop_words.find(word) == stop_words.end()) {
                word_frequency[word]++;
            }
        }
    }
    
    // Find consensus points (words appearing in majority of results)
    ss << "Key consensus points:\n";
    std::vector<std::pair<std::string, size_t>> consensus_words;
    
    for (const auto& [word, count] : word_frequency) {
        if (count > results.size() / 2) {
            consensus_words.push_back({word, count});
        }
    }
    
    std::sort(consensus_words.begin(), consensus_words.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (size_t i = 0; i < std::min(size_t(10), consensus_words.size()); ++i) {
        ss << "- " << consensus_words[i].first << " (mentioned " << 
              consensus_words[i].second << " times)\n";
    }
    
    ss << "\nDetailed perspectives:\n\n";
    
    // Include all perspectives
    for (size_t i = 0; i < results.size(); ++i) {
        ss << "Perspective " << (i + 1) << ":\n";
        ss << results[i].result_text << "\n\n";
    }
    
    return ss.str();
}

std::string ParallelStrategy::weightedCombineAggregator(const std::vector<SubtaskResult>& results) {
    if (results.empty()) return "";
    
    std::stringstream ss;
    ss << "Weighted combination of " << results.size() << " results:\n\n";
    
    // Sort by weight * quality
    std::vector<SubtaskResult> sorted_results = results;
    std::sort(sorted_results.begin(), sorted_results.end(),
        [](const auto& a, const auto& b) {
            double weight_a = 1.0;
            double weight_b = 1.0;
            
            if (a.metadata.count("weight") > 0) {
                weight_a = std::stod(a.metadata.at("weight"));
            }
            if (b.metadata.count("weight") > 0) {
                weight_b = std::stod(b.metadata.at("weight"));
            }
            
            return (weight_a * a.quality_score) > (weight_b * b.quality_score);
        });
    
    // Include results weighted by importance
    double total_weight = 0.0;
    for (const auto& result : sorted_results) {
        double weight = 1.0;
        if (result.metadata.count("weight") > 0) {
            weight = std::stod(result.metadata.at("weight"));
        }
        
        double importance = weight * result.quality_score;
        total_weight += importance;
        
        ss << "### " << result.subtask_id << " (importance: " << 
              std::fixed << std::setprecision(2) << importance << ")\n\n";
        ss << result.result_text << "\n\n";
    }
    
    return ss.str();
}

// Utility functions

std::string parallelPatternToString(ParallelPattern pattern) {
    switch (pattern) {
        case ParallelPattern::FILE_ANALYSIS:
            return "file_analysis";
        case ParallelPattern::MULTI_ASPECT_REVIEW:
            return "multi_aspect_review";
        case ParallelPattern::ALTERNATIVE_GENERATION:
            return "alternative_generation";
        case ParallelPattern::VALIDATION_SUITE:
            return "validation_suite";
        case ParallelPattern::CUSTOM:
            return "custom";
        default:
            return "unknown";
    }
}

ParallelPattern stringToParallelPattern(const std::string& str) {
    if (str == "file_analysis") return ParallelPattern::FILE_ANALYSIS;
    if (str == "multi_aspect_review") return ParallelPattern::MULTI_ASPECT_REVIEW;
    if (str == "alternative_generation") return ParallelPattern::ALTERNATIVE_GENERATION;
    if (str == "validation_suite") return ParallelPattern::VALIDATION_SUITE;
    if (str == "custom") return ParallelPattern::CUSTOM;
    
    return ParallelPattern::CUSTOM;
}

std::string aggregationMethodToString(AggregationMethod method) {
    switch (method) {
        case AggregationMethod::CONCATENATE:
            return "concatenate";
        case AggregationMethod::MERGE:
            return "merge";
        case AggregationMethod::BEST_QUALITY:
            return "best_quality";
        case AggregationMethod::CONSENSUS:
            return "consensus";
        case AggregationMethod::WEIGHTED_COMBINE:
            return "weighted_combine";
        case AggregationMethod::CUSTOM:
            return "custom";
        default:
            return "unknown";
    }
}

AggregationMethod stringToAggregationMethod(const std::string& str) {
    if (str == "concatenate") return AggregationMethod::CONCATENATE;
    if (str == "merge") return AggregationMethod::MERGE;
    if (str == "best_quality") return AggregationMethod::BEST_QUALITY;
    if (str == "consensus") return AggregationMethod::CONSENSUS;
    if (str == "weighted_combine") return AggregationMethod::WEIGHTED_COMBINE;
    if (str == "custom") return AggregationMethod::CUSTOM;
    
    return AggregationMethod::MERGE;
}

} // namespace Camus