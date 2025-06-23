// =================================================================
// src/Camus/ModelPool.cpp
// =================================================================
// Implementation of the model pool for managing multiple LLM instances.

#include "Camus/ModelPool.hpp"
#include "Camus/ModelCapabilities.hpp"
#include <algorithm>
#include <stdexcept>

namespace Camus {

ConcreteModelPool::ConcreteModelPool() {
    m_stats.last_update = std::chrono::system_clock::now();
}

ConcreteModelPool::~ConcreteModelPool() {
    cleanupAll();
}

bool ConcreteModelPool::addModel(std::shared_ptr<LlmInteraction> model) {
    if (!model) {
        return false;
    }
    
    std::string model_id = model->getModelId();
    if (model_id.empty()) {
        return false;
    }
    
    // Check if model with this ID already exists
    if (m_models.find(model_id) != m_models.end()) {
        return false;
    }
    
    m_models[model_id] = model;
    updateStats();
    return true;
}

bool ConcreteModelPool::removeModel(const std::string& model_id) {
    auto it = m_models.find(model_id);
    if (it == m_models.end()) {
        return false;
    }
    
    // Clean up the model before removing
    it->second->cleanup();
    m_models.erase(it);
    updateStats();
    return true;
}

std::shared_ptr<LlmInteraction> ConcreteModelPool::getModel(const std::string& model_id) {
    auto it = m_models.find(model_id);
    if (it != m_models.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<LlmInteraction> ConcreteModelPool::selectModel(const ModelSelectionCriteria& criteria) {
    auto candidates = getModelsMatching(criteria);
    if (candidates.empty()) {
        return nullptr;
    }
    
    // Use custom selector if available, otherwise use default
    if (m_custom_selector) {
        return m_custom_selector(candidates, criteria);
    } else {
        return defaultModelSelection(candidates, criteria);
    }
}

std::vector<std::shared_ptr<LlmInteraction>> ConcreteModelPool::getModelsMatching(const ModelSelectionCriteria& criteria) {
    std::vector<std::shared_ptr<LlmInteraction>> matching_models;
    
    for (const auto& [model_id, model] : m_models) {
        if (!model) continue;
        
        // Check availability requirement
        if (criteria.require_availability && !model->isHealthy()) {
            continue;
        }
        
        auto metadata = model->getModelMetadata();
        auto performance = model->getCurrentPerformance();
        
        // Check required capabilities
        bool has_required_capabilities = true;
        for (const auto& required_cap : criteria.required_capabilities) {
            if (!ModelCapabilityUtils::hasCapability(metadata, required_cap)) {
                has_required_capabilities = false;
                break;
            }
        }
        if (!has_required_capabilities) {
            continue;
        }
        
        // Check context token requirements
        if (performance.max_context_tokens < criteria.min_context_tokens ||
            performance.max_context_tokens > criteria.max_context_tokens) {
            continue;
        }
        
        // Check latency requirements
        if (performance.avg_latency.count() > criteria.max_latency_ms) {
            continue;
        }
        
        matching_models.push_back(model);
    }
    
    return matching_models;
}

std::vector<std::string> ConcreteModelPool::getAllModelIds() const {
    std::vector<std::string> model_ids;
    model_ids.reserve(m_models.size());
    
    for (const auto& [model_id, model] : m_models) {
        model_ids.push_back(model_id);
    }
    
    return model_ids;
}

std::vector<ModelMetadata> ConcreteModelPool::getAllModelMetadata() const {
    std::vector<ModelMetadata> metadata_list;
    metadata_list.reserve(m_models.size());
    
    for (const auto& [model_id, model] : m_models) {
        if (model) {
            metadata_list.push_back(model->getModelMetadata());
        }
    }
    
    return metadata_list;
}

size_t ConcreteModelPool::performHealthChecks() {
    size_t healthy_count = 0;
    
    for (const auto& [model_id, model] : m_models) {
        if (model && model->performHealthCheck()) {
            healthy_count++;
        }
    }
    
    updateStats();
    return healthy_count;
}

ModelPoolStats ConcreteModelPool::getPoolStats() const {
    return m_stats;
}

size_t ConcreteModelPool::warmUpAll() {
    size_t warmed_up_count = 0;
    
    for (const auto& [model_id, model] : m_models) {
        if (model && model->warmUp()) {
            warmed_up_count++;
        }
    }
    
    return warmed_up_count;
}

void ConcreteModelPool::cleanupAll() {
    for (const auto& [model_id, model] : m_models) {
        if (model) {
            model->cleanup();
        }
    }
    m_models.clear();
    updateStats();
}

bool ConcreteModelPool::isEmpty() const {
    return m_models.empty();
}

size_t ConcreteModelPool::size() const {
    return m_models.size();
}

void ConcreteModelPool::setCustomSelector(std::function<std::shared_ptr<LlmInteraction>(
    const std::vector<std::shared_ptr<LlmInteraction>>&, 
    const ModelSelectionCriteria&)> selector) {
    m_custom_selector = selector;
}

std::shared_ptr<LlmInteraction> ConcreteModelPool::defaultModelSelection(
    const std::vector<std::shared_ptr<LlmInteraction>>& candidates,
    const ModelSelectionCriteria& criteria) {
    
    if (candidates.empty()) {
        return nullptr;
    }
    
    // Score each candidate model
    std::vector<std::pair<std::shared_ptr<LlmInteraction>, double>> scored_models;
    for (const auto& model : candidates) {
        double score = calculateModelScore(model, criteria);
        scored_models.emplace_back(model, score);
    }
    
    // Sort by score (highest first)
    std::sort(scored_models.begin(), scored_models.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return scored_models[0].first;
}

double ConcreteModelPool::calculateModelScore(
    const std::shared_ptr<LlmInteraction>& model,
    const ModelSelectionCriteria& criteria) {
    
    if (!model) return 0.0;
    
    auto metadata = model->getModelMetadata();
    auto performance = model->getCurrentPerformance();
    
    double score = 0.0;
    
    // Base score for availability
    if (metadata.is_healthy && metadata.is_available) {
        score += 10.0;
    }
    
    // Score for preferred capabilities
    for (const auto& preferred_cap : criteria.preferred_capabilities) {
        if (ModelCapabilityUtils::hasCapability(metadata, preferred_cap)) {
            score += 5.0;
        }
    }
    
    // Score based on speed vs quality preference
    if (criteria.prefer_speed) {
        // Higher score for faster models
        if (performance.tokens_per_second > 0) {
            score += std::min(10.0, performance.tokens_per_second / 10.0);
        }
        if (performance.avg_latency.count() > 0) {
            score += std::max(0.0, 10.0 - (performance.avg_latency.count() / 1000.0));
        }
    } else {
        // Higher score for quality models
        if (ModelCapabilityUtils::hasCapability(metadata, ModelCapability::HIGH_QUALITY)) {
            score += 15.0;
        }
        if (ModelCapabilityUtils::hasCapability(metadata, ModelCapability::REASONING)) {
            score += 10.0;
        }
    }
    
    // Score for context window size (prefer larger context when not specified as max)
    if (criteria.max_context_tokens == SIZE_MAX) {
        score += std::min(5.0, performance.max_context_tokens / 8192.0);
    }
    
    // Penalty for high resource usage
    score -= performance.memory_usage_gb * 0.5;
    score -= performance.cpu_usage_percent * 0.1;
    
    return std::max(0.0, score);
}

void ConcreteModelPool::updateStats() {
    m_stats.total_models = m_models.size();
    m_stats.available_models = 0;
    m_stats.healthy_models = 0;
    m_stats.last_update = std::chrono::system_clock::now();
    
    for (const auto& [model_id, model] : m_models) {
        if (model) {
            auto metadata = model->getModelMetadata();
            if (metadata.is_available) {
                m_stats.available_models++;
            }
            if (metadata.is_healthy) {
                m_stats.healthy_models++;
            }
        }
    }
}

} // namespace Camus