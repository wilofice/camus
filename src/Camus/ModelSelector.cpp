// =================================================================
// src/Camus/ModelSelector.cpp
// =================================================================
// Implementation of intelligent model selector.

#include "Camus/ModelSelector.hpp"
#include "Camus/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Camus {

// =================================================================
// Built-in Selection Strategies
// =================================================================

/**
 * @brief Rule-based selection strategy
 */
class ModelSelector::RuleBasedStrategy : public SelectionStrategy {
public:
    SelectionResult selectModel(
        const SelectionCriteria& criteria,
        const std::vector<ModelConfig>& available_models,
        const std::unordered_map<std::string, ModelStatistics>& model_stats
    ) override {
        auto start_time = std::chrono::steady_clock::now();
        SelectionResult result;
        
        // Apply rules in order of priority
        for (const auto& model : available_models) {
            bool matches = true;
            
            // Rule 1: Task type compatibility
            if (criteria.task_type == TaskType::CODE_GENERATION ||
                criteria.task_type == TaskType::CODE_ANALYSIS ||
                criteria.task_type == TaskType::BUG_FIXING) {
                
                auto it = std::find(model.capabilities.begin(), model.capabilities.end(), "CODE_SPECIALIZED");
                if (it == model.capabilities.end()) {
                    matches = false;
                }
            }
            
            // Rule 2: Context size requirements
            if (criteria.context_size > model.max_tokens) {
                matches = false;
            }
            
            // Rule 3: Quality preference
            if (criteria.prefer_quality) {
                auto it = std::find(model.capabilities.begin(), model.capabilities.end(), "HIGH_QUALITY");
                if (it == model.capabilities.end()) {
                    matches = false;
                }
            }
            
            // Rule 4: Required capabilities
            for (const auto& req_cap : criteria.required_capabilities) {
                auto it = std::find(model.capabilities.begin(), model.capabilities.end(), req_cap);
                if (it == model.capabilities.end()) {
                    matches = false;
                    break;
                }
            }
            
            // Rule 5: Latency requirements
            if (criteria.max_latency_ms > 0) {
                auto stats_it = model_stats.find(model.name);
                if (stats_it != model_stats.end() && 
                    stats_it->second.average_latency_ms > criteria.max_latency_ms) {
                    matches = false;
                }
            }
            
            if (matches) {
                result.selected_model = model.name;
                result.confidence_score = 0.85; // High confidence for rule-based
                result.selection_reason = "Model matches all rule-based criteria";
                break;
            }
        }
        
        // If no perfect match, find best alternative
        if (result.selected_model.empty() && !available_models.empty()) {
            result.selected_model = available_models[0].name;
            result.confidence_score = 0.5;
            result.selection_reason = "Default fallback - no models matched all rules";
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.selection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        return result;
    }
    
    std::string getName() const override {
        return "rule_based";
    }
};

/**
 * @brief Score-based selection strategy
 */
class ModelSelector::ScoreBasedStrategy : public SelectionStrategy {
public:
    SelectionResult selectModel(
        const SelectionCriteria& criteria,
        const std::vector<ModelConfig>& available_models,
        const std::unordered_map<std::string, ModelStatistics>& model_stats
    ) override {
        auto start_time = std::chrono::steady_clock::now();
        SelectionResult result;
        
        std::vector<std::pair<std::string, double>> model_scores;
        
        for (const auto& model : available_models) {
            double score = 0.0;
            
            // Capability matching score (0-40 points)
            double capability_score = 0.0;
            int matched_capabilities = 0;
            
            for (const auto& cap : criteria.required_capabilities) {
                if (std::find(model.capabilities.begin(), model.capabilities.end(), cap) 
                    != model.capabilities.end()) {
                    matched_capabilities++;
                }
            }
            
            if (!criteria.required_capabilities.empty()) {
                capability_score = (matched_capabilities / 
                    static_cast<double>(criteria.required_capabilities.size())) * 40.0;
            } else {
                capability_score = 20.0; // Neutral score if no requirements
            }
            
            // Context size score (0-20 points)
            double context_score = 0.0;
            if (model.max_tokens >= criteria.context_size) {
                context_score = 20.0;
            } else {
                context_score = (model.max_tokens / static_cast<double>(criteria.context_size)) * 20.0;
            }
            
            // Performance score (0-20 points)
            double performance_score = 10.0; // Default neutral score
            auto stats_it = model_stats.find(model.name);
            if (stats_it != model_stats.end()) {
                const auto& stats = stats_it->second;
                if (stats.total_requests > 0) {
                    performance_score = stats.success_rate * 20.0;
                    
                    // Adjust for latency if specified
                    if (criteria.max_latency_ms > 0 && stats.average_latency_ms > 0) {
                        double latency_ratio = criteria.max_latency_ms / stats.average_latency_ms;
                        performance_score *= std::min(1.0, latency_ratio);
                    }
                }
            }
            
            // Quality score (0-20 points)
            double quality_score = 10.0; // Default neutral score
            if (criteria.prefer_quality) {
                if (std::find(model.capabilities.begin(), model.capabilities.end(), "HIGH_QUALITY") 
                    != model.capabilities.end()) {
                    quality_score = 20.0;
                }
            } else if (criteria.max_latency_ms > 0) {
                // Prefer faster models if latency is a concern
                if (std::find(model.capabilities.begin(), model.capabilities.end(), "FAST_INFERENCE") 
                    != model.capabilities.end()) {
                    quality_score = 20.0;
                }
            }
            
            // Calculate total score
            score = capability_score + context_score + performance_score + quality_score;
            
            // Apply custom weights if provided
            if (!criteria.custom_weights.empty()) {
                double weight_multiplier = 1.0;
                for (const auto& [key, weight] : criteria.custom_weights) {
                    if (key == model.type || key == model.name) {
                        weight_multiplier *= weight;
                    }
                }
                score *= weight_multiplier;
            }
            
            // Normalize to 0-1 range
            score /= 100.0;
            
            model_scores.push_back({model.name, score});
        }
        
        // Sort by score descending
        std::sort(model_scores.begin(), model_scores.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (!model_scores.empty()) {
            result.selected_model = model_scores[0].first;
            result.confidence_score = model_scores[0].second;
            
            // Build selection reason
            std::ostringstream reason;
            reason << "Score-based selection (score: " 
                   << std::fixed << std::setprecision(2) 
                   << model_scores[0].second << ")";
            result.selection_reason = reason.str();
            
            // Add alternatives
            for (size_t i = 1; i < std::min(size_t(3), model_scores.size()); ++i) {
                result.alternatives.push_back(model_scores[i]);
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.selection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        return result;
    }
    
    std::string getName() const override {
        return "score_based";
    }
};

/**
 * @brief Learning-based selection strategy
 */
class ModelSelector::LearningBasedStrategy : public SelectionStrategy {
public:
    SelectionResult selectModel(
        const SelectionCriteria& criteria,
        const std::vector<ModelConfig>& available_models,
        const std::unordered_map<std::string, ModelStatistics>& model_stats
    ) override {
        auto start_time = std::chrono::steady_clock::now();
        SelectionResult result;
        
        std::vector<std::pair<std::string, double>> model_scores;
        
        for (const auto& model : available_models) {
            double score = 0.0;
            
            // Base score from model capabilities
            double base_score = 0.5;
            
            // Adjust based on historical performance
            auto stats_it = model_stats.find(model.name);
            if (stats_it != model_stats.end()) {
                const auto& stats = stats_it->second;
                
                // Overall success rate weight
                if (stats.total_requests > 0) {
                    base_score = stats.success_rate * 0.4;
                    
                    // Task-specific performance
                    std::string task_key = taskTypeToString(criteria.task_type);
                    auto task_perf_it = stats.task_performance.find(task_key);
                    if (task_perf_it != stats.task_performance.end()) {
                        base_score += task_perf_it->second * 0.3;
                    }
                    
                    // Quality score weight
                    if (criteria.prefer_quality && stats.average_quality_score > 0) {
                        base_score += stats.average_quality_score * 0.3;
                    }
                    
                    // Recency bias - prefer recently successful models
                    auto time_since_use = std::chrono::duration_cast<std::chrono::minutes>(
                        std::chrono::system_clock::now() - stats.last_used).count();
                    if (time_since_use < 60) { // Used within last hour
                        base_score *= 1.1;
                    }
                }
            }
            
            // Capability matching bonus
            int matched_caps = 0;
            for (const auto& cap : criteria.required_capabilities) {
                if (std::find(model.capabilities.begin(), model.capabilities.end(), cap) 
                    != model.capabilities.end()) {
                    matched_caps++;
                }
            }
            if (!criteria.required_capabilities.empty()) {
                double cap_ratio = matched_caps / static_cast<double>(criteria.required_capabilities.size());
                base_score *= (1.0 + cap_ratio * 0.5);
            }
            
            // Context size penalty if insufficient
            if (model.max_tokens < criteria.context_size) {
                base_score *= 0.5;
            }
            
            score = std::min(1.0, base_score);
            model_scores.push_back({model.name, score});
        }
        
        // Sort by score descending
        std::sort(model_scores.begin(), model_scores.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (!model_scores.empty()) {
            result.selected_model = model_scores[0].first;
            result.confidence_score = model_scores[0].second;
            
            // Build selection reason
            std::ostringstream reason;
            reason << "Learning-based selection using historical performance (confidence: " 
                   << std::fixed << std::setprecision(2) 
                   << model_scores[0].second << ")";
            result.selection_reason = reason.str();
            
            // Add alternatives
            for (size_t i = 1; i < std::min(size_t(3), model_scores.size()); ++i) {
                result.alternatives.push_back(model_scores[i]);
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.selection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        return result;
    }
    
    std::string getName() const override {
        return "learning_based";
    }
};

// =================================================================
// ModelSelector Implementation
// =================================================================

ModelSelector::ModelSelector(ModelRegistry& registry, const ModelSelectorConfig& config)
    : m_registry(registry), m_config(config) {
    
    // Register built-in strategies
    registerStrategy("rule_based", std::make_shared<RuleBasedStrategy>());
    registerStrategy("score_based", std::make_shared<ScoreBasedStrategy>());
    registerStrategy("learning_based", std::make_shared<LearningBasedStrategy>());
    
    // Set default strategy
    setActiveStrategy(m_config.default_strategy);
    
    Logger::getInstance().info("ModelSelector", "Initialized with strategy: " + m_active_strategy);
}

ModelSelector::~ModelSelector() = default;

SelectionResult ModelSelector::selectModel(const SelectionCriteria& criteria) {
    // Get available models
    auto available_models = m_registry.getConfiguredModels();
    
    // Filter out unhealthy models
    std::vector<ModelConfig> healthy_models;
    for (const auto& model : available_models) {
        if (m_registry.getModel(model.name) != nullptr) {
            healthy_models.push_back(model);
        }
    }
    
    if (healthy_models.empty()) {
        Logger::getInstance().error("ModelSelector", "No healthy models available");
        SelectionResult result;
        result.selection_reason = "No healthy models available";
        return result;
    }
    
    // Get current statistics
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    // Use active strategy
    auto strategy_it = m_strategies.find(m_active_strategy);
    if (strategy_it == m_strategies.end()) {
        Logger::getInstance().error("ModelSelector", "Active strategy not found: " + m_active_strategy);
        SelectionResult result;
        result.selection_reason = "Selection strategy error";
        return result;
    }
    
    auto result = strategy_it->second->selectModel(criteria, healthy_models, m_model_stats);
    
    // Log selection decision
    Logger::getInstance().info("ModelSelector", 
        "Selected model: " + result.selected_model + 
        " (confidence: " + std::to_string(result.confidence_score) + ")");
    
    // Record in history
    m_selection_history.push_back(result);
    if (m_selection_history.size() > m_config.performance_history_size) {
        m_selection_history.erase(m_selection_history.begin());
    }
    
    return result;
}

void ModelSelector::registerStrategy(const std::string& name, 
                                   std::shared_ptr<SelectionStrategy> strategy) {
    m_strategies[name] = strategy;
    Logger::getInstance().info("ModelSelector", "Registered strategy: " + name);
}

bool ModelSelector::setActiveStrategy(const std::string& strategy_name) {
    if (m_strategies.find(strategy_name) != m_strategies.end()) {
        m_active_strategy = strategy_name;
        Logger::getInstance().info("ModelSelector", "Active strategy set to: " + strategy_name);
        return true;
    }
    return false;
}

std::string ModelSelector::getActiveStrategy() const {
    return m_active_strategy;
}

void ModelSelector::recordPerformance(const std::string& model_name,
                                    TaskType task_type,
                                    bool success,
                                    double latency_ms,
                                    double quality_score) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    auto& stats = m_model_stats[model_name];
    updateStatistics(stats, success, latency_ms, quality_score);
    
    // Update task-specific performance
    std::string task_key = taskTypeToString(task_type);
    double& task_perf = stats.task_performance[task_key];
    
    // Running average for task performance
    if (stats.total_requests == 1) {
        task_perf = success ? 1.0 : 0.0;
    } else {
        task_perf = (task_perf * (stats.total_requests - 1) + (success ? 1.0 : 0.0)) 
                    / stats.total_requests;
    }
    
    Logger::getInstance().debug("ModelSelector", 
        "Recorded performance for " + model_name + 
        " - Success: " + (success ? "true" : "false") +
        ", Latency: " + std::to_string(latency_ms) + "ms");
}

ModelStatistics ModelSelector::getModelStatistics(const std::string& model_name) const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    auto it = m_model_stats.find(model_name);
    if (it != m_model_stats.end()) {
        return it->second;
    }
    
    return ModelStatistics();
}

std::unordered_map<std::string, ModelStatistics> ModelSelector::getAllStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_model_stats;
}

void ModelSelector::clearStatistics(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_model_stats.erase(model_name);
    Logger::getInstance().info("ModelSelector", "Cleared statistics for: " + model_name);
}

void ModelSelector::clearAllStatistics() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_model_stats.clear();
    m_selection_history.clear();
    Logger::getInstance().info("ModelSelector", "Cleared all statistics");
}

ModelSelectorConfig ModelSelector::getConfig() const {
    return m_config;
}

void ModelSelector::setConfig(const ModelSelectorConfig& config) {
    m_config = config;
    setActiveStrategy(config.default_strategy);
    Logger::getInstance().info("ModelSelector", "Configuration updated");
}

std::vector<SelectionResult> ModelSelector::getSelectionHistory(size_t limit) const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    std::vector<SelectionResult> history;
    size_t start = m_selection_history.size() > limit ? 
                   m_selection_history.size() - limit : 0;
    
    for (size_t i = start; i < m_selection_history.size(); ++i) {
        history.push_back(m_selection_history[i]);
    }
    
    return history;
}

std::string ModelSelector::analyzeSelectionPatterns() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    std::ostringstream analysis;
    analysis << "Model Selection Analysis\n";
    analysis << "========================\n\n";
    
    // Model usage frequency
    std::unordered_map<std::string, size_t> usage_count;
    for (const auto& selection : m_selection_history) {
        usage_count[selection.selected_model]++;
    }
    
    analysis << "Model Usage Frequency:\n";
    for (const auto& [model, count] : usage_count) {
        double percentage = (count * 100.0) / m_selection_history.size();
        analysis << "  " << model << ": " << count << " times (" 
                 << std::fixed << std::setprecision(1) << percentage << "%)\n";
    }
    
    // Average confidence scores
    analysis << "\nAverage Confidence Scores:\n";
    std::unordered_map<std::string, double> confidence_sum;
    std::unordered_map<std::string, size_t> confidence_count;
    
    for (const auto& selection : m_selection_history) {
        confidence_sum[selection.selected_model] += selection.confidence_score;
        confidence_count[selection.selected_model]++;
    }
    
    for (const auto& [model, sum] : confidence_sum) {
        double avg = sum / confidence_count[model];
        analysis << "  " << model << ": " 
                 << std::fixed << std::setprecision(3) << avg << "\n";
    }
    
    // Model performance summary
    analysis << "\nModel Performance Summary:\n";
    for (const auto& [model, stats] : m_model_stats) {
        analysis << "  " << model << ":\n";
        analysis << "    Total requests: " << stats.total_requests << "\n";
        analysis << "    Success rate: " 
                 << std::fixed << std::setprecision(1) 
                 << (stats.success_rate * 100) << "%\n";
        analysis << "    Avg latency: " 
                 << std::fixed << std::setprecision(0) 
                 << stats.average_latency_ms << "ms\n";
        if (stats.average_quality_score > 0) {
            analysis << "    Avg quality: " 
                     << std::fixed << std::setprecision(2) 
                     << stats.average_quality_score << "\n";
        }
    }
    
    return analysis.str();
}

double ModelSelector::calculateModelScore(const ModelConfig& model,
                                        const SelectionCriteria& criteria,
                                        const ModelStatistics& stats) {
    // This is used by derived classes if needed
    // Base implementation provides a simple scoring mechanism
    double score = 0.5; // Start with neutral score
    
    // Capability matching
    for (const auto& req_cap : criteria.required_capabilities) {
        if (std::find(model.capabilities.begin(), model.capabilities.end(), req_cap) 
            != model.capabilities.end()) {
            score += 0.1;
        }
    }
    
    // Historical performance
    if (stats.total_requests > 0) {
        score = score * 0.5 + stats.success_rate * 0.5;
    }
    
    return std::min(1.0, score);
}

bool ModelSelector::meetsRequirements(const ModelConfig& model,
                                    const SelectionCriteria& criteria) const {
    // Check context size
    if (model.max_tokens < criteria.context_size) {
        return false;
    }
    
    // Check required capabilities
    for (const auto& req_cap : criteria.required_capabilities) {
        if (std::find(model.capabilities.begin(), model.capabilities.end(), req_cap) 
            == model.capabilities.end()) {
            return false;
        }
    }
    
    return true;
}

void ModelSelector::updateStatistics(ModelStatistics& stats,
                                   bool success,
                                   double latency_ms,
                                   double quality_score) {
    stats.total_requests++;
    if (success) {
        stats.successful_requests++;
    }
    
    // Update success rate
    stats.success_rate = static_cast<double>(stats.successful_requests) / stats.total_requests;
    
    // Update average latency (running average)
    if (stats.total_requests == 1) {
        stats.average_latency_ms = latency_ms;
    } else {
        stats.average_latency_ms = 
            (stats.average_latency_ms * (stats.total_requests - 1) + latency_ms) 
            / stats.total_requests;
    }
    
    // Update quality score if provided
    if (quality_score >= 0) {
        if (stats.average_quality_score == 0) {
            stats.average_quality_score = quality_score;
        } else {
            stats.average_quality_score = 
                (stats.average_quality_score * (stats.total_requests - 1) + quality_score) 
                / stats.total_requests;
        }
    }
    
    stats.last_used = std::chrono::system_clock::now();
}

} // namespace Camus