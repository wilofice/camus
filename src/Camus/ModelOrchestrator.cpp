// =================================================================
// src/Camus/ModelOrchestrator.cpp
// =================================================================
// Implementation of request routing pipeline orchestrator.

#include "Camus/ModelOrchestrator.hpp"
#include "Camus/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <regex>
#include <future>
#include <unordered_set>

namespace Camus {

ModelOrchestrator::ModelOrchestrator(ModelRegistry& registry, const OrchestratorConfig& config)
    : m_registry(registry), m_config(config) {
    
    // Initialize components
    if (m_config.enable_classification) {
        ClassificationConfig classifier_config;
        m_classifier = std::make_unique<TaskClassifier>(classifier_config);
        Logger::getInstance().info("ModelOrchestrator", "Task classifier initialized");
    }
    
    if (m_config.enable_model_selection) {
        ModelSelectorConfig selector_config;
        m_selector = std::make_unique<ModelSelector>(registry, selector_config);
        Logger::getInstance().info("ModelOrchestrator", "Model selector initialized");
    }
    
    if (m_config.enable_load_balancing) {
        LoadBalancerConfig lb_config;
        m_load_balancer = std::make_unique<LoadBalancer>(registry, lb_config);
        Logger::getInstance().info("ModelOrchestrator", "Load balancer initialized");
    }
    
    // Set default quality scorer
    m_quality_scorer = [this](const std::string& prompt, const std::string& response) {
        return defaultQualityScorer(prompt, response);
    };
    
    // Initialize statistics
    m_statistics.last_reset = std::chrono::system_clock::now();
    
    // Start background cleanup thread if caching is enabled
    if (m_config.enable_caching) {
        startCleanupThread();
    }
    
    Logger::getInstance().info("ModelOrchestrator", "Orchestrator initialized successfully");
}

ModelOrchestrator::~ModelOrchestrator() {
    stopCleanupThread();
}

PipelineResponse ModelOrchestrator::processRequest(const PipelineRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    PipelineResponse response;
    response.request_id = request.request_id;
    response.pipeline_steps.push_back("pipeline_start");
    
    Logger::getInstance().debug("ModelOrchestrator", 
        "Processing request: " + request.request_id);
    
    try {
        // Step 1: Check cache first
        if (m_config.enable_caching && request.enable_caching) {
            response.pipeline_steps.push_back("cache_check");
            if (checkCache(request, response)) {
                response.success = true;
                response.cache_hit = true;
                response.pipeline_steps.push_back("cache_hit");
                
                auto end_time = std::chrono::steady_clock::now();
                response.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);
                
                updateStatistics(request, response);
                
                Logger::getInstance().info("ModelOrchestrator", 
                    "Cache hit for request: " + request.request_id);
                return response;
            }
            response.pipeline_steps.push_back("cache_miss");
        }
        
        // Step 2: Task classification
        ClassificationResult classification_result;
        if (m_config.enable_classification && m_classifier) {
            response.pipeline_steps.push_back("task_classification");
            classification_result = classifyTask(request, response);
            
            if (classification_result.confidence < m_config.min_classification_confidence) {
                Logger::getInstance().warning("ModelOrchestrator", 
                    "Low classification confidence: " + std::to_string(classification_result.confidence));
            }
        } else {
            // Default classification
            classification_result.primary_type = TaskType::UNKNOWN;
            classification_result.confidence = 1.0;
            response.classified_task = TaskType::UNKNOWN;
            response.classification_confidence = 1.0;
        }
        
        // Step 3: Model selection
        SelectionResult selection_result;
        if (m_config.enable_model_selection && m_selector) {
            response.pipeline_steps.push_back("model_selection");
            selection_result = selectModel(request, classification_result, response);
            
            if (selection_result.selected_model.empty()) {
                throw std::runtime_error("No suitable model found for request");
            }
            
            if (selection_result.confidence_score < m_config.min_selection_confidence) {
                Logger::getInstance().warning("ModelOrchestrator", 
                    "Low selection confidence: " + std::to_string(selection_result.confidence_score));
            }
        } else {
            // Use any available model
            auto models = m_registry.getConfiguredModels();
            if (models.empty()) {
                throw std::runtime_error("No models available in registry");
            }
            selection_result.selected_model = models[0].name;
            selection_result.confidence_score = 0.5;
            selection_result.selection_reason = "Default model selection";
        }
        
        // Step 4: Load balancing
        LoadBalancingResult lb_result;
        if (m_config.enable_load_balancing && m_load_balancer) {
            response.pipeline_steps.push_back("load_balancing");
            lb_result = selectInstance(request, selection_result.selected_model, response);
            
            if (lb_result.selected_instance_id.empty() || !lb_result.model) {
                throw std::runtime_error("No healthy instances available for model: " + 
                                       selection_result.selected_model);
            }
        } else {
            // Direct model access
            auto model = m_registry.getModel(selection_result.selected_model);
            if (!model) {
                throw std::runtime_error("Model not available: " + selection_result.selected_model);
            }
            lb_result.model = model;
            lb_result.selected_instance_id = selection_result.selected_model + "_direct";
            lb_result.selection_reason = "Direct model access";
        }
        
        // Step 5: Request execution
        response.pipeline_steps.push_back("request_execution");
        bool execution_success = executeRequest(request, lb_result, response);
        
        if (!execution_success) {
            if (m_config.enable_fallback && request.require_fallback) {
                response.pipeline_steps.push_back("fallback_handling");
                execution_success = handleFallback(request, response);
                if (execution_success) {
                    response.fallback_used = true;
                }
            }
            
            if (!execution_success) {
                throw std::runtime_error("Request execution failed and fallback unsuccessful");
            }
        }
        
        // Step 6: Response validation and quality scoring
        if (m_config.enable_quality_checks) {
            response.pipeline_steps.push_back("quality_validation");
            response.quality_score = validateResponse(request, response);
            
            if (response.quality_score < m_config.min_quality_score) {
                Logger::getInstance().warning("ModelOrchestrator", 
                    "Low quality response: " + std::to_string(response.quality_score));
                
                if (m_config.fallback_on_low_quality && request.require_fallback && 
                    !response.fallback_used) {
                    response.pipeline_steps.push_back("quality_fallback");
                    if (handleFallback(request, response)) {
                        response.fallback_used = true;
                        response.quality_score = validateResponse(request, response);
                    }
                }
            }
        }
        
        response.success = true;
        response.pipeline_steps.push_back("pipeline_complete");
        
        // Step 7: Cache the response
        if (m_config.enable_caching && request.enable_caching && !response.cache_hit) {
            storeInCache(request, response);
        }
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = e.what();
        response.pipeline_steps.push_back("pipeline_error");
        
        Logger::getInstance().error("ModelOrchestrator", 
            "Request processing failed: " + response.error_message + 
            " (request: " + request.request_id + ")");
        
        // Try fallback on error
        if (m_config.enable_fallback && m_config.fallback_on_error && 
            request.require_fallback && !response.fallback_used) {
            try {
                response.pipeline_steps.push_back("error_fallback");
                if (handleFallback(request, response)) {
                    response.success = true;
                    response.fallback_used = true;
                    response.error_message = "";
                }
            } catch (const std::exception& fallback_error) {
                Logger::getInstance().error("ModelOrchestrator", 
                    "Fallback also failed: " + std::string(fallback_error.what()));
            }
        }
    }
    
    // Calculate total time
    auto end_time = std::chrono::steady_clock::now();
    response.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    // Update statistics
    updateStatistics(request, response);
    
    Logger::getInstance().info("ModelOrchestrator", 
        "Request completed: " + request.request_id + 
        " (success: " + (response.success ? "true" : "false") + 
        ", time: " + std::to_string(response.total_time.count()) + "ms)");
    
    return response;
}

std::vector<PipelineResponse> ModelOrchestrator::processRequests(
    const std::vector<PipelineRequest>& requests) {
    
    std::vector<std::future<PipelineResponse>> futures;
    
    // Process requests asynchronously
    for (const auto& request : requests) {
        futures.push_back(std::async(std::launch::async, 
            [this, request]() { return processRequest(request); }));
    }
    
    // Collect results
    std::vector<PipelineResponse> responses;
    for (auto& future : futures) {
        responses.push_back(future.get());
    }
    
    Logger::getInstance().info("ModelOrchestrator", 
        "Processed batch of " + std::to_string(requests.size()) + " requests");
    
    return responses;
}

ClassificationResult ModelOrchestrator::classifyTask(const PipelineRequest& request, 
                                                    PipelineResponse& response) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::string text_to_classify = request.prompt;
    if (!request.context.empty()) {
        text_to_classify += " " + request.context;
    }
    
    auto result = m_classifier->classify(text_to_classify);
    
    auto end_time = std::chrono::steady_clock::now();
    response.classification_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    response.classified_task = result.primary_type;
    response.classification_confidence = result.confidence;
    
    Logger::getInstance().debug("ModelOrchestrator", 
        "Task classified as: " + taskTypeToString(result.primary_type) + 
        " (confidence: " + std::to_string(result.confidence) + ")");
    
    return result;
}

SelectionResult ModelOrchestrator::selectModel(const PipelineRequest& request,
                                              const ClassificationResult& task_result,
                                              PipelineResponse& response) {
    auto start_time = std::chrono::steady_clock::now();
    
    SelectionCriteria criteria;
    criteria.task_type = task_result.primary_type;
    criteria.context_size = request.prompt.length() + request.context.length();
    criteria.expected_output_size = request.max_tokens;
    
    // Apply user preferences
    if (!request.preferred_models.empty()) {
        for (const auto& model : request.preferred_models) {
            criteria.custom_weights[model] = 2.0; // Higher weight for preferred models
        }
    }
    
    auto result = m_selector->selectModel(criteria);
    
    auto end_time = std::chrono::steady_clock::now();
    response.selection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    response.selected_model = result.selected_model;
    response.selection_confidence = result.confidence_score;
    
    Logger::getInstance().debug("ModelOrchestrator", 
        "Selected model: " + result.selected_model + 
        " (confidence: " + std::to_string(result.confidence_score) + ")");
    
    return result;
}

LoadBalancingResult ModelOrchestrator::selectInstance(const PipelineRequest& request,
                                                     const std::string& model_name,
                                                     PipelineResponse& response) {
    RequestContext lb_context;
    lb_context.request_id = request.request_id;
    lb_context.prompt = request.prompt;
    lb_context.estimated_tokens = request.prompt.length() / 4; // Rough estimate
    lb_context.max_response_time = request.timeout;
    lb_context.priority = request.priority;
    
    auto result = m_load_balancer->selectInstance(model_name, lb_context);
    
    response.selected_instance = result.selected_instance_id;
    
    Logger::getInstance().debug("ModelOrchestrator", 
        "Selected instance: " + result.selected_instance_id + " for model: " + model_name);
    
    return result;
}

bool ModelOrchestrator::executeRequest(const PipelineRequest& request,
                                      const LoadBalancingResult& lb_result,
                                      PipelineResponse& response) {
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Record request start for load balancing
        if (m_load_balancer && !lb_result.selected_instance_id.empty()) {
            m_load_balancer->recordRequestStart(lb_result.selected_instance_id, request.request_id);
        }
        
        // Execute the request
        std::string model_response = lb_result.model->getCompletion(request.prompt);
        
        auto end_time = std::chrono::steady_clock::now();
        response.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        response.response_text = model_response;
        response.tokens_generated = model_response.length() / 4; // Rough estimate
        
        // Record request completion for load balancing
        if (m_load_balancer && !lb_result.selected_instance_id.empty()) {
            m_load_balancer->recordRequestEnd(lb_result.selected_instance_id, request.request_id, 
                                            response.execution_time.count(), true);
        }
        
        Logger::getInstance().debug("ModelOrchestrator", 
            "Request executed successfully in " + std::to_string(response.execution_time.count()) + "ms");
        
        return true;
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        response.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        response.error_message = "Execution failed: " + std::string(e.what());
        
        // Record request failure for load balancing
        if (m_load_balancer && !lb_result.selected_instance_id.empty()) {
            m_load_balancer->recordRequestEnd(lb_result.selected_instance_id, request.request_id, 
                                            response.execution_time.count(), false);
        }
        
        Logger::getInstance().error("ModelOrchestrator", response.error_message);
        return false;
    }
}

double ModelOrchestrator::validateResponse(const PipelineRequest& request, PipelineResponse& response) {
    if (response.response_text.empty()) {
        return 0.0;
    }
    
    double quality_score = m_quality_scorer(request.prompt, response.response_text);
    
    Logger::getInstance().debug("ModelOrchestrator", 
        "Response quality score: " + std::to_string(quality_score));
    
    return quality_score;
}

bool ModelOrchestrator::handleFallback(const PipelineRequest& request, PipelineResponse& response) {
    Logger::getInstance().info("ModelOrchestrator", "Attempting fallback for request: " + request.request_id);
    
    try {
        switch (m_fallback_strategy) {
            case FallbackStrategy::SIMPLE_MODEL: {
                // Try to find a simpler, faster model
                auto models = m_registry.getConfiguredModels();
                for (const auto& model : models) {
                    if (model.name != response.selected_model) {
                        auto fallback_model = m_registry.getModel(model.name);
                        if (fallback_model) {
                            response.response_text = fallback_model->getCompletion(request.prompt);
                            response.selected_model = model.name;
                            response.selected_instance = model.name + "_fallback";
                            return true;
                        }
                    }
                }
                break;
            }
            
            case FallbackStrategy::FASTEST_MODEL: {
                // Use the model with fastest expected response time
                auto models = m_registry.getConfiguredModels();
                std::string fastest_model = "";
                double fastest_time = std::numeric_limits<double>::max();
                
                for (const auto& model : models) {
                    if (model.expected_latency_ms < fastest_time && model.name != response.selected_model) {
                        fastest_time = model.expected_latency_ms;
                        fastest_model = model.name;
                    }
                }
                
                if (!fastest_model.empty()) {
                    auto fallback_model = m_registry.getModel(fastest_model);
                    if (fallback_model) {
                        response.response_text = fallback_model->getCompletion(request.prompt);
                        response.selected_model = fastest_model;
                        response.selected_instance = fastest_model + "_fallback";
                        return true;
                    }
                }
                break;
            }
            
            case FallbackStrategy::CACHED_RESPONSE: {
                // Try to find a similar cached response
                std::lock_guard<std::mutex> lock(m_cache_mutex);
                double best_similarity = 0.0;
                std::string best_response = "";
                
                for (const auto& [key, entry] : m_cache) {
                    // Extract prompt from cache key (simplified)
                    size_t pos = key.find('|');
                    if (pos != std::string::npos) {
                        std::string cached_prompt = key.substr(0, pos);
                        double similarity = calculatePromptSimilarity(request.prompt, cached_prompt);
                        if (similarity > best_similarity && similarity > 0.7) {
                            best_similarity = similarity;
                            best_response = entry.response_text;
                        }
                    }
                }
                
                if (!best_response.empty()) {
                    response.response_text = best_response;
                    response.selected_model = "cached_fallback";
                    response.selected_instance = "cache";
                    return true;
                }
                break;
            }
            
            case FallbackStrategy::DEFAULT_RESPONSE: {
                response.response_text = "I apologize, but I'm unable to process your request at the moment. Please try again later.";
                response.selected_model = "default_fallback";
                response.selected_instance = "default";
                return true;
            }
            
            case FallbackStrategy::NONE:
            default:
                return false;
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("ModelOrchestrator", 
            "Fallback strategy failed: " + std::string(e.what()));
    }
    
    return false;
}

bool ModelOrchestrator::checkCache(const PipelineRequest& request, PipelineResponse& response) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    std::string cache_key = generateCacheKey(request);
    
    auto it = m_cache.find(cache_key);
    if (it != m_cache.end()) {
        auto& entry = it->second;
        
        // Check if entry is still valid
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.created_at);
        
        if (age < m_config.cache_ttl) {
            response.response_text = entry.response_text;
            response.selected_model = entry.model_used;
            response.selected_instance = "cache";
            response.quality_score = entry.quality_score;
            
            // Update access info
            entry.last_accessed = now;
            entry.access_count++;
            
            return true;
        } else {
            // Remove expired entry
            m_cache.erase(it);
        }
    }
    
    return false;
}

void ModelOrchestrator::storeInCache(const PipelineRequest& request, const PipelineResponse& response) {
    if (response.response_text.empty() || 
        request.prompt.length() < m_config.min_prompt_length_for_cache) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    // Check cache size limit
    if (m_cache.size() >= m_config.max_cache_size) {
        // Remove oldest entry
        auto oldest_it = std::min_element(m_cache.begin(), m_cache.end(),
            [](const auto& a, const auto& b) {
                return a.second.last_accessed < b.second.last_accessed;
            });
        
        if (oldest_it != m_cache.end()) {
            m_cache.erase(oldest_it);
        }
    }
    
    std::string cache_key = generateCacheKey(request);
    
    CacheEntry entry;
    entry.response_text = response.response_text;
    entry.model_used = response.selected_model;
    entry.created_at = std::chrono::system_clock::now();
    entry.last_accessed = entry.created_at;
    entry.access_count = 0;
    entry.quality_score = response.quality_score;
    
    m_cache[cache_key] = entry;
    
    Logger::getInstance().debug("ModelOrchestrator", "Response cached with key: " + cache_key);
}

std::string ModelOrchestrator::generateCacheKey(const PipelineRequest& request) {
    std::ostringstream key;
    key << request.prompt << "|" 
        << request.max_tokens << "|" 
        << std::fixed << std::setprecision(2) << request.temperature;
    
    if (!request.context.empty()) {
        key << "|ctx:" << request.context;
    }
    
    return key.str();
}

double ModelOrchestrator::calculatePromptSimilarity(const std::string& prompt1, const std::string& prompt2) {
    // Simple word-based similarity calculation
    if (prompt1.empty() || prompt2.empty()) {
        return 0.0;
    }
    
    // Convert to lowercase and split into words
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };
    
    std::string p1 = toLower(prompt1);
    std::string p2 = toLower(prompt2);
    
    std::regex word_regex(R"(\w+)");
    std::sregex_iterator words1_begin(p1.begin(), p1.end(), word_regex);
    std::sregex_iterator words2_begin(p2.begin(), p2.end(), word_regex);
    std::sregex_iterator words_end;
    
    std::unordered_set<std::string> set1, set2;
    
    for (auto it = words1_begin; it != words_end; ++it) {
        set1.insert(it->str());
    }
    
    for (auto it = words2_begin; it != words_end; ++it) {
        set2.insert(it->str());
    }
    
    if (set1.empty() || set2.empty()) {
        return 0.0;
    }
    
    // Calculate Jaccard similarity
    std::unordered_set<std::string> intersection, union_set;
    
    for (const auto& word : set1) {
        union_set.insert(word);
        if (set2.count(word)) {
            intersection.insert(word);
        }
    }
    
    for (const auto& word : set2) {
        union_set.insert(word);
    }
    
    return static_cast<double>(intersection.size()) / union_set.size();
}

void ModelOrchestrator::cleanupCache() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    auto now = std::chrono::system_clock::now();
    size_t removed_count = 0;
    
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.created_at);
        if (age > m_config.cache_ttl) {
            it = m_cache.erase(it);
            removed_count++;
        } else {
            ++it;
        }
    }
    
    if (removed_count > 0) {
        Logger::getInstance().debug("ModelOrchestrator", 
            "Cleaned up " + std::to_string(removed_count) + " expired cache entries");
    }
}

void ModelOrchestrator::updateStatistics(const PipelineRequest& request, const PipelineResponse& response) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_statistics.total_requests++;
    
    if (response.success) {
        m_statistics.successful_requests++;
    } else {
        m_statistics.failed_requests++;
    }
    
    if (response.cache_hit) {
        m_statistics.cache_hits++;
    }
    
    if (response.fallback_used) {
        m_statistics.fallback_used++;
    }
    
    // Update average response time
    if (m_statistics.total_requests == 1) {
        m_statistics.average_response_time = response.total_time.count();
    } else {
        m_statistics.average_response_time = 
            (m_statistics.average_response_time * (m_statistics.total_requests - 1) + 
             response.total_time.count()) / m_statistics.total_requests;
    }
    
    // Update average quality score
    if (response.quality_score > 0.0) {
        if (m_statistics.average_quality_score == 0.0) {
            m_statistics.average_quality_score = response.quality_score;
        } else {
            m_statistics.average_quality_score = 
                (m_statistics.average_quality_score * (m_statistics.total_requests - 1) + 
                 response.quality_score) / m_statistics.total_requests;
        }
    }
    
    // Update model usage
    if (!response.selected_model.empty()) {
        m_statistics.model_usage[response.selected_model]++;
    }
    
    // Update task distribution
    m_statistics.task_distribution[response.classified_task]++;
}

OrchestratorConfig ModelOrchestrator::getConfig() const {
    return m_config;
}

void ModelOrchestrator::setConfig(const OrchestratorConfig& config) {
    m_config = config;
    Logger::getInstance().info("ModelOrchestrator", "Configuration updated");
}

PipelineStatistics ModelOrchestrator::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_statistics;
}

void ModelOrchestrator::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_statistics = PipelineStatistics();
    m_statistics.last_reset = std::chrono::system_clock::now();
    Logger::getInstance().info("ModelOrchestrator", "Statistics reset");
}

void ModelOrchestrator::clearCache() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    m_cache.clear();
    Logger::getInstance().info("ModelOrchestrator", "Cache cleared");
}

std::unordered_map<std::string, double> ModelOrchestrator::getCacheStatistics() const {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    
    std::unordered_map<std::string, double> stats;
    
    stats["cache_size"] = static_cast<double>(m_cache.size());
    stats["cache_hit_rate"] = m_statistics.total_requests > 0 ? 
        static_cast<double>(m_statistics.cache_hits) / m_statistics.total_requests : 0.0;
    
    size_t total_accesses = 0;
    for (const auto& [key, entry] : m_cache) {
        total_accesses += entry.access_count;
    }
    stats["average_access_count"] = m_cache.empty() ? 0.0 : 
        static_cast<double>(total_accesses) / m_cache.size();
    
    return stats;
}

void ModelOrchestrator::setFallbackStrategy(FallbackStrategy strategy) {
    m_fallback_strategy = strategy;
    Logger::getInstance().info("ModelOrchestrator", 
        "Fallback strategy set to: " + std::to_string(static_cast<int>(strategy)));
}

FallbackStrategy ModelOrchestrator::getFallbackStrategy() const {
    return m_fallback_strategy;
}

void ModelOrchestrator::registerQualityScorer(
    std::function<double(const std::string&, const std::string&)> scorer) {
    m_quality_scorer = scorer;
    Logger::getInstance().info("ModelOrchestrator", "Custom quality scorer registered");
}

void ModelOrchestrator::warmupPipeline(const std::vector<std::string>& sample_requests) {
    Logger::getInstance().info("ModelOrchestrator", 
        "Warming up pipeline with " + std::to_string(sample_requests.size()) + " sample requests");
    
    for (size_t i = 0; i < sample_requests.size(); ++i) {
        PipelineRequest request;
        request.request_id = "warmup_" + std::to_string(i);
        request.prompt = sample_requests[i];
        request.enable_caching = false; // Don't cache warmup requests
        
        try {
            processRequest(request);
        } catch (const std::exception& e) {
            Logger::getInstance().warning("ModelOrchestrator", 
                "Warmup request failed: " + std::string(e.what()));
        }
    }
    
    Logger::getInstance().info("ModelOrchestrator", "Pipeline warmup completed");
}

std::string ModelOrchestrator::getPerformanceReport() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    std::ostringstream report;
    report << "Model Orchestrator Performance Report\n";
    report << "=====================================\n\n";
    
    report << "Request Statistics:\n";
    report << "  Total Requests: " << m_statistics.total_requests << "\n";
    report << "  Successful: " << m_statistics.successful_requests << "\n";
    report << "  Failed: " << m_statistics.failed_requests << "\n";
    report << "  Success Rate: " << std::fixed << std::setprecision(1)
           << (m_statistics.total_requests > 0 ? 
               (m_statistics.successful_requests * 100.0) / m_statistics.total_requests : 0.0) 
           << "%\n\n";
    
    report << "Performance Metrics:\n";
    report << "  Average Response Time: " << std::fixed << std::setprecision(0)
           << m_statistics.average_response_time << "ms\n";
    report << "  Average Quality Score: " << std::fixed << std::setprecision(2)
           << m_statistics.average_quality_score << "\n";
    report << "  Cache Hit Rate: " << std::fixed << std::setprecision(1)
           << (m_statistics.total_requests > 0 ? 
               (m_statistics.cache_hits * 100.0) / m_statistics.total_requests : 0.0) 
           << "%\n";
    report << "  Fallback Usage: " << m_statistics.fallback_used << " times\n\n";
    
    report << "Model Usage:\n";
    for (const auto& [model, count] : m_statistics.model_usage) {
        double percentage = (count * 100.0) / m_statistics.total_requests;
        report << "  " << model << ": " << count << " times (" 
               << std::fixed << std::setprecision(1) << percentage << "%)\n";
    }
    
    report << "\nTask Distribution:\n";
    for (const auto& [task, count] : m_statistics.task_distribution) {
        double percentage = (count * 100.0) / m_statistics.total_requests;
        report << "  " << taskTypeToString(task) << ": " << count << " times (" 
               << std::fixed << std::setprecision(1) << percentage << "%)\n";
    }
    
    return report.str();
}

void ModelOrchestrator::setComponentEnabled(const std::string& component, bool enabled) {
    if (component == "classification") {
        m_config.enable_classification = enabled;
    } else if (component == "model_selection") {
        m_config.enable_model_selection = enabled;
    } else if (component == "load_balancing") {
        m_config.enable_load_balancing = enabled;
    } else if (component == "caching") {
        m_config.enable_caching = enabled;
        if (enabled) {
            startCleanupThread();
        } else {
            stopCleanupThread();
        }
    } else if (component == "fallback") {
        m_config.enable_fallback = enabled;
    } else if (component == "quality_checks") {
        m_config.enable_quality_checks = enabled;
    }
    
    Logger::getInstance().info("ModelOrchestrator", 
        "Component " + component + " " + (enabled ? "enabled" : "disabled"));
}

double ModelOrchestrator::defaultQualityScorer(const std::string& prompt, const std::string& response) {
    if (response.empty()) {
        return 0.0;
    }
    
    double score = 0.5; // Base score
    
    // Length appropriateness (not too short, not too long)
    double length_ratio = static_cast<double>(response.length()) / prompt.length();
    if (length_ratio >= 0.5 && length_ratio <= 5.0) {
        score += 0.2;
    }
    
    // Check for common error patterns
    std::string lower_response = response;
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(), ::tolower);
    
    if (lower_response.find("error") == std::string::npos &&
        lower_response.find("sorry") == std::string::npos &&
        lower_response.find("unable") == std::string::npos) {
        score += 0.2;
    }
    
    // Basic coherence check (simple sentence structure)
    size_t sentence_count = std::count(response.begin(), response.end(), '.') +
                           std::count(response.begin(), response.end(), '!') +
                           std::count(response.begin(), response.end(), '?');
    
    if (sentence_count > 0) {
        score += 0.1;
    }
    
    return std::min(1.0, score);
}

void ModelOrchestrator::startCleanupThread() {
    if (!m_cleanup_thread) {
        m_stop_cleanup.store(false);
        m_cleanup_thread = std::make_unique<std::thread>(&ModelOrchestrator::cacheCleanupLoop, this);
        Logger::getInstance().info("ModelOrchestrator", "Started cache cleanup thread");
    }
}

void ModelOrchestrator::stopCleanupThread() {
    if (m_cleanup_thread) {
        m_stop_cleanup.store(true);
        m_cleanup_thread->join();
        m_cleanup_thread.reset();
        Logger::getInstance().info("ModelOrchestrator", "Stopped cache cleanup thread");
    }
}

void ModelOrchestrator::cacheCleanupLoop() {
    while (!m_stop_cleanup.load()) {
        try {
            cleanupCache();
        } catch (const std::exception& e) {
            Logger::getInstance().error("ModelOrchestrator", 
                "Cache cleanup error: " + std::string(e.what()));
        }
        
        // Sleep for 5 minutes between cleanups
        std::this_thread::sleep_for(std::chrono::minutes(5));
    }
}

} // namespace Camus