// =================================================================
// src/Camus/SequentialStrategy.cpp
// =================================================================
// Implementation of sequential strategy for chained model processing.

#include "Camus/SequentialStrategy.hpp"
#include "Camus/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <numeric>
#include <chrono>
#include <future>
#include <thread>

namespace Camus {

SequentialStrategy::SequentialStrategy(ModelRegistry& registry, 
                                     const SequentialStrategyConfig& config)
    : m_registry(registry), m_config(config) {
    
    // Initialize default templates and prompts
    initializeDefaultTemplates();
    initializeDefaultPrompts();
    
    // Initialize statistics
    m_statistics.last_reset = std::chrono::system_clock::now();
    
    Logger::getInstance().info("SequentialStrategy", "Strategy initialized successfully");
}

SequentialResponse SequentialStrategy::execute(const SequentialRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    SequentialResponse response;
    response.request_id = request.request_id;
    response.pattern_used = request.pattern;
    response.success = false;
    
    // Initialize pipeline state
    PipelineState state;
    state.request_id = request.request_id;
    state.original_input = request.prompt;
    state.current_output = request.original_code.empty() ? request.prompt : request.original_code;
    state.start_time = std::chrono::system_clock::now();
    
    // Add context to state
    if (!request.context.empty()) {
        state.context_variables["context"] = request.context;
    }
    state.context_variables["task_type"] = taskTypeToString(request.task_type);
    
    try {
        // Check cache if enabled
        if (request.enable_caching && checkCache(request, state)) {
            response.final_output = state.current_output;
            response.success = true;
            response.final_state = state;
            
            Logger::getInstance().debug("SequentialStrategy", 
                "Using cached result for request: " + request.request_id);
        } else {
            // Execute pipeline based on format mode
            bool execution_success = false;
            
            if (request.original_format_mode || !request.backend_names.empty()) {
                // Use original pipeline format
                execution_success = executeOriginalPipeline(request, state);
                response.total_steps = request.backend_names.empty() ? 
                    m_config.named_pipelines[request.pipeline_name].size() : 
                    request.backend_names.size();
            } else {
                // Use enhanced pipeline format
                execution_success = executeEnhancedPipeline(request, state);
                response.total_steps = request.custom_steps.size();
            }
            
            response.success = execution_success;
            response.final_output = state.current_output;
            response.final_state = state;
            response.steps_completed = state.current_step_index;
            
            // Store in cache if successful and caching enabled
            if (execution_success && request.enable_caching) {
                storeInCache(request, state);
            }
        }
        
        // Copy execution history
        response.transformation_history = state.processing_history;
        response.step_quality_scores = state.quality_scores;
        response.overall_quality_score = calculateOverallQuality(state);
        
        // Extract step timings from debug info
        for (const auto& [key, value] : state.context_variables) {
            if (key.find("step_timing_") == 0) {
                std::string step_id = key.substr(12); // Remove "step_timing_" prefix
                response.step_timings[step_id] = std::chrono::milliseconds(std::stoll(value));
            }
        }
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = "Pipeline execution failed: " + std::string(e.what());
        response.final_state = state;
        
        Logger::getInstance().error("SequentialStrategy", 
            "Pipeline execution failed: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::steady_clock::now();
    response.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    // Calculate coordination overhead
    auto total_step_time = std::accumulate(
        response.step_timings.begin(), response.step_timings.end(),
        std::chrono::milliseconds(0),
        [](const auto& acc, const auto& pair) { return acc + pair.second; });
    response.coordination_time = response.total_time - total_step_time;
    
    // Update statistics
    updateStatistics(request, response);
    
    return response;
}

bool SequentialStrategy::executeOriginalPipeline(const SequentialRequest& request, 
                                                PipelineState& state) {
    // Get backend names from request or named pipeline
    std::vector<std::string> backend_names = request.backend_names;
    if (backend_names.empty() && !request.pipeline_name.empty()) {
        auto it = m_config.named_pipelines.find(request.pipeline_name);
        if (it != m_config.named_pipelines.end()) {
            backend_names = it->second;
        } else {
            throw std::runtime_error("Named pipeline not found: " + request.pipeline_name);
        }
    }
    
    if (backend_names.empty()) {
        throw std::runtime_error("No backend names specified for pipeline");
    }
    
    // Convert backend names to pipeline steps
    auto steps = convertBackendListToSteps(backend_names, request);
    
    // Execute each step in sequence
    std::string current_code = request.original_code;
    if (current_code.empty()) {
        current_code = request.prompt;
    }
    
    for (size_t i = 0; i < steps.size(); ++i) {
        auto step_start = std::chrono::steady_clock::now();
        
        // Log original format console output
        if (m_config.enable_console_output) {
            std::string log_msg = "[INFO] Pipeline Step " + std::to_string(i + 1) + "/" + 
                                 std::to_string(steps.size()) + ": Using model '" + 
                                 steps[i].model_name + "'...";
            state.processing_history.push_back(log_msg);
            Logger::getInstance().info("SequentialStrategy", log_msg);
        }
        
        // Update state for current step
        state.current_step_index = i;
        state.current_output = current_code;
        
        // Execute the step
        bool step_success = executeStep(steps[i], state);
        
        if (!step_success) {
            state.has_errors = true;
            state.error_messages.push_back("Step " + std::to_string(i + 1) + " failed");
            
            if (steps[i].is_required) {
                return false;
            }
        } else {
            // Update current code for next iteration
            current_code = state.current_output;
        }
        
        auto step_end = std::chrono::steady_clock::now();
        auto step_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            step_end - step_start);
        
        state.context_variables["step_timing_step_" + std::to_string(i)] = 
            std::to_string(step_duration.count());
    }
    
    return true;
}

bool SequentialStrategy::executeEnhancedPipeline(const SequentialRequest& request, 
                                               PipelineState& state) {
    // Get steps based on pattern or custom steps
    std::vector<PipelineStep> steps;
    if (!request.custom_steps.empty()) {
        steps = request.custom_steps;
    } else {
        steps = getStepsForPattern(request.pattern, request);
    }
    
    if (steps.empty()) {
        throw std::runtime_error("No pipeline steps defined");
    }
    
    // Execute each step with enhanced features
    for (size_t i = 0; i < steps.size(); ++i) {
        auto step_start = std::chrono::steady_clock::now();
        
        const auto& step = steps[i];
        state.current_step_index = i;
        
        Logger::getInstance().debug("SequentialStrategy", 
            "Executing step " + step.step_id + ": " + step.step_description);
        
        // Execute the step
        bool step_success = executeStep(step, state);
        
        if (!step_success) {
            state.has_errors = true;
            state.error_messages.push_back("Step '" + step.step_id + "' failed");
            
            // Handle failure with potential rollback
            if (request.enable_rollback && !state.step_outputs.empty()) {
                bool recovery_success = handleStepFailure(step, state, "Step execution failed");
                if (!recovery_success && step.is_required) {
                    return false;
                }
            } else if (step.is_required) {
                return false;
            }
        } else {
            // Validate step output if enabled
            if (request.enable_validation && step.min_quality_threshold > 0) {
                bool validation_success = validateStepOutput(step, state.current_output, state);
                if (!validation_success) {
                    state.has_errors = true;
                    state.error_messages.push_back("Step '" + step.step_id + "' validation failed");
                    
                    if (request.enable_rollback) {
                        bool recovery_success = handleStepFailure(step, state, "Validation failed");
                        if (!recovery_success && step.is_required) {
                            return false;
                        }
                    } else if (step.is_required) {
                        return false;
                    }
                }
            }
            
            // Store step output
            state.step_outputs[step.step_id] = state.current_output;
            
            // Cache result if requested
            if (step.cache_result && request.enable_caching) {
                // Create a temporary request for caching this step
                SequentialRequest step_request = request;
                step_request.request_id = request.request_id + "_step_" + step.step_id;
                storeInCache(step_request, state);
            }
        }
        
        auto step_end = std::chrono::steady_clock::now();
        auto step_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            step_end - step_start);
        
        state.context_variables["step_timing_" + step.step_id] = 
            std::to_string(step_duration.count());
    }
    
    return !state.has_errors || state.error_messages.empty();
}

bool SequentialStrategy::executeStep(const PipelineStep& step, PipelineState& state) {
    try {
        // Get model from registry
        auto model = m_registry.getModel(step.model_name);
        if (!model) {
            // Try fallback model if specified
            if (!step.fallback_model.empty()) {
                Logger::getInstance().warn("SequentialStrategy", 
                    "Primary model '" + step.model_name + "' unavailable, trying fallback: " + 
                    step.fallback_model);
                model = m_registry.getModel(step.fallback_model);
            }
            
            if (!model) {
                throw std::runtime_error("Model not available: " + step.model_name);
            }
        }
        
        // Build prompt for this step
        std::string prompt = buildStepPrompt(step, state);
        
        // Execute model with timeout
        std::future<std::string> future = std::async(std::launch::async, 
            [&model, &prompt]() {
                return model->getCompletion(prompt);
            });
        
        std::string result;
        if (future.wait_for(step.timeout) == std::future_status::timeout) {
            throw std::runtime_error("Step execution timed out after " + 
                                   std::to_string(step.timeout.count()) + "ms");
        }
        
        result = future.get();
        
        if (result.empty()) {
            throw std::runtime_error("Model returned empty response");
        }
        
        // Update state with result
        state.current_output = result;
        state.processing_history.push_back("Step '" + step.step_id + "' completed by " + 
                                         step.model_name);
        
        // Calculate and store quality score
        double quality = calculateStepQuality(step, result, state.original_input);
        state.quality_scores[step.step_id] = quality;
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("SequentialStrategy", 
            "Step '" + step.step_id + "' failed: " + std::string(e.what()));
        return false;
    }
}

bool SequentialStrategy::validateStepOutput(const PipelineStep& step, 
                                           const std::string& output, 
                                           PipelineState& state) {
    // Check quality threshold
    double quality = state.quality_scores[step.step_id];
    if (quality < step.min_quality_threshold) {
        Logger::getInstance().warn("SequentialStrategy", 
            "Step '" + step.step_id + "' quality " + std::to_string(quality) + 
            " below threshold " + std::to_string(step.min_quality_threshold));
        return false;
    }
    
    // Apply validation rules
    for (const auto& rule : step.validation_rules) {
        bool rule_passed = false;
        
        // Simple validation rule examples
        if (rule == "non_empty") {
            rule_passed = !output.empty();
        } else if (rule == "contains_code") {
            rule_passed = (output.find("```") != std::string::npos) ||
                         (output.find("function") != std::string::npos) ||
                         (output.find("class") != std::string::npos);
        } else if (rule == "no_errors") {
            rule_passed = (output.find("error") == std::string::npos) &&
                         (output.find("Error") == std::string::npos) &&
                         (output.find("ERROR") == std::string::npos);
        } else if (rule.find("min_length:") == 0) {
            size_t min_length = std::stoul(rule.substr(11));
            rule_passed = output.length() >= min_length;
        } else if (rule.find("max_length:") == 0) {
            size_t max_length = std::stoul(rule.substr(11));
            rule_passed = output.length() <= max_length;
        } else if (rule.find("contains:") == 0) {
            std::string required_text = rule.substr(9);
            rule_passed = output.find(required_text) != std::string::npos;
        }
        
        if (!rule_passed) {
            Logger::getInstance().warn("SequentialStrategy", 
                "Step '" + step.step_id + "' failed validation rule: " + rule);
            return false;
        }
    }
    
    return true;
}

bool SequentialStrategy::handleStepFailure(const PipelineStep& step, 
                                          PipelineState& state, 
                                          const std::string& error_message) {
    Logger::getInstance().warn("SequentialStrategy", 
        "Handling failure for step '" + step.step_id + "': " + error_message);
    
    // Try rollback to previous successful state
    if (!state.step_outputs.empty()) {
        // Find the last successful step
        std::string last_good_output;
        std::string last_good_step;
        
        for (auto it = state.processing_history.rbegin(); 
             it != state.processing_history.rend(); ++it) {
            if (it->find("completed") != std::string::npos) {
                // Extract step ID from history
                size_t start = it->find("'") + 1;
                size_t end = it->find("'", start);
                if (start != std::string::npos && end != std::string::npos) {
                    last_good_step = it->substr(start, end - start);
                    auto output_it = state.step_outputs.find(last_good_step);
                    if (output_it != state.step_outputs.end()) {
                        last_good_output = output_it->second;
                        break;
                    }
                }
            }
        }
        
        if (!last_good_output.empty()) {
            Logger::getInstance().info("SequentialStrategy", 
                "Rolling back to last successful state from step: " + last_good_step);
            state.current_output = last_good_output;
            state.processing_history.push_back("Rolled back to step '" + last_good_step + 
                                             "' after failure");
            return true;
        }
    }
    
    // No rollback possible
    return false;
}

std::vector<PipelineStep> SequentialStrategy::getStepsForPattern(
    SequentialPattern pattern, const SequentialRequest& request) {
    
    // Check for pre-configured pattern templates
    auto it = m_config.pattern_templates.find(pattern);
    if (it != m_config.pattern_templates.end()) {
        return it->second;
    }
    
    // Generate default steps based on pattern
    std::vector<PipelineStep> steps;
    
    switch (pattern) {
        case SequentialPattern::ORIGINAL_REFINEMENT: {
            // Original progressive refinement pattern
            steps.push_back({
                "initial_generation",
                "llama3-local",
                "Initial code generation",
                "{{{original_input}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(30000),
                0.5,
                {"non_empty"},
                ""
            });
            steps.push_back({
                "refinement",
                "deepseek-coder-local",
                "Code refinement and optimization",
                "Refine and optimize the following code:\n\n{{{current_output}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(30000),
                0.6,
                {"non_empty", "contains_code"},
                ""
            });
            break;
        }
        
        case SequentialPattern::ANALYSIS_GENERATION: {
            steps.push_back({
                "analysis",
                "llama3-local",
                "Analyze requirements and create plan",
                "Analyze the following request and create a detailed implementation plan:\n\n{{{original_input}}}",
                {},
                true,
                true,
                std::chrono::milliseconds(30000),
                0.7,
                {"non_empty", "min_length:100"},
                ""
            });
            steps.push_back({
                "generation",
                "deepseek-coder-local",
                "Generate code based on analysis",
                "Based on this analysis:\n{{{current_output}}}\n\nGenerate the implementation for:\n{{{original_input}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(45000),
                0.8,
                {"non_empty", "contains_code"},
                ""
            });
            break;
        }
        
        case SequentialPattern::GENERATION_VALIDATION: {
            steps.push_back({
                "generation",
                "deepseek-coder-local",
                "Generate initial implementation",
                "{{{original_input}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(30000),
                0.6,
                {"non_empty", "contains_code"},
                ""
            });
            steps.push_back({
                "validation",
                "llama3-local",
                "Validate and improve code quality",
                "Review the following code for quality, security, and best practices. Provide an improved version:\n\n{{{current_output}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(30000),
                0.8,
                {"non_empty", "no_errors"},
                ""
            });
            break;
        }
        
        case SequentialPattern::PLANNING_IMPLEMENTATION: {
            steps.push_back({
                "planning",
                "llama3-local",
                "Create detailed implementation plan",
                "Create a detailed step-by-step plan for: {{{original_input}}}",
                {},
                true,
                true,
                std::chrono::milliseconds(30000),
                0.7,
                {"non_empty", "min_length:200"},
                ""
            });
            steps.push_back({
                "implementation",
                "deepseek-coder-local",
                "Implement based on plan",
                "Implement the following plan:\n{{{current_output}}}\n\nOriginal request: {{{original_input}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(60000),
                0.8,
                {"non_empty", "contains_code"},
                ""
            });
            break;
        }
        
        case SequentialPattern::REVIEW_REFINEMENT: {
            steps.push_back({
                "initial_implementation",
                "deepseek-coder-local",
                "Create initial implementation",
                "{{{original_input}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(30000),
                0.5,
                {"non_empty"},
                ""
            });
            steps.push_back({
                "review",
                "llama3-local",
                "Review and suggest improvements",
                "Review the following implementation and suggest improvements:\n\n{{{current_output}}}\n\nOriginal requirements: {{{original_input}}}",
                {},
                true,
                true,
                std::chrono::milliseconds(30000),
                0.7,
                {"non_empty"},
                ""
            });
            steps.push_back({
                "refinement",
                "deepseek-coder-local",
                "Apply improvements",
                "Apply the following improvements to the code:\n{{{current_output}}}",
                {},
                true,
                false,
                std::chrono::milliseconds(30000),
                0.8,
                {"non_empty", "contains_code"},
                ""
            });
            break;
        }
        
        case SequentialPattern::CUSTOM:
        default:
            // Return empty vector for custom pattern
            break;
    }
    
    return steps;
}

std::vector<PipelineStep> SequentialStrategy::convertBackendListToSteps(
    const std::vector<std::string>& backend_names,
    const SequentialRequest& request) {
    
    std::vector<PipelineStep> steps;
    
    for (size_t i = 0; i < backend_names.size(); ++i) {
        PipelineStep step;
        step.step_id = "step_" + std::to_string(i + 1);
        step.model_name = backend_names[i];
        step.step_description = "Pipeline step " + std::to_string(i + 1);
        
        // First step uses original prompt, subsequent steps use previous output
        if (i == 0) {
            step.prompt_template = "{{{original_input}}}";
        } else {
            step.prompt_template = "Refine and improve the following:\n\n{{{current_output}}}";
        }
        
        step.is_required = true;
        step.cache_result = false;
        step.timeout = m_config.default_step_timeout;
        step.min_quality_threshold = m_config.min_step_quality;
        
        steps.push_back(step);
    }
    
    return steps;
}

std::string SequentialStrategy::buildStepPrompt(const PipelineStep& step, 
                                               const PipelineState& state) {
    // Create variable map for template substitution
    std::unordered_map<std::string, std::string> variables;
    
    // Add state variables
    variables["original_input"] = state.original_input;
    variables["current_output"] = state.current_output;
    variables["request_id"] = state.request_id;
    
    // Add all context variables
    for (const auto& [key, value] : state.context_variables) {
        variables[key] = value;
    }
    
    // Add step outputs
    for (const auto& [step_id, output] : state.step_outputs) {
        variables["output_" + step_id] = output;
    }
    
    // Add step parameters
    for (const auto& [key, value] : step.step_parameters) {
        variables[key] = value;
    }
    
    // Apply template substitution
    std::string prompt = step.prompt_template;
    if (!prompt.empty()) {
        prompt = applyTemplate(prompt, variables);
    } else {
        // Use default prompt if no template specified
        if (state.current_step_index == 0) {
            prompt = state.original_input;
        } else {
            prompt = "Continue processing:\n\n" + state.current_output;
        }
    }
    
    return prompt;
}

bool SequentialStrategy::checkCache(const SequentialRequest& request, 
                                   PipelineState& state) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    // Clean up expired entries periodically
    cleanupCache();
    
    std::string cache_key = generateCacheKey(request);
    auto it = m_cache.find(cache_key);
    
    if (it != m_cache.end()) {
        auto& cached = it->second;
        
        // Check if cache entry is still valid
        auto age = std::chrono::system_clock::now() - cached.created_at;
        if (age <= m_config.cache_ttl) {
            // Use cached result
            state.current_output = cached.cached_output;
            state = cached.cached_state;
            
            // Update access info
            cached.last_used = std::chrono::system_clock::now();
            cached.access_count++;
            
            return true;
        } else {
            // Remove expired entry
            m_cache.erase(it);
        }
    }
    
    return false;
}

void SequentialStrategy::storeInCache(const SequentialRequest& request, 
                                     const PipelineState& state) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    // Check cache size limit
    if (m_cache.size() >= m_config.max_cache_size) {
        // Remove least recently used entry
        auto lru_it = std::min_element(m_cache.begin(), m_cache.end(),
            [](const auto& a, const auto& b) {
                return a.second.last_used < b.second.last_used;
            });
        
        if (lru_it != m_cache.end()) {
            m_cache.erase(lru_it);
        }
    }
    
    // Store new entry
    std::string cache_key = generateCacheKey(request);
    CachedResult cached;
    cached.input_hash = cache_key;
    cached.cached_output = state.current_output;
    cached.cached_state = state;
    cached.created_at = std::chrono::system_clock::now();
    cached.last_used = cached.created_at;
    cached.access_count = 1;
    cached.quality_score = calculateOverallQuality(state);
    
    m_cache[cache_key] = cached;
}

std::string SequentialStrategy::generateCacheKey(const SequentialRequest& request) {
    std::stringstream ss;
    
    // Include relevant request fields in cache key
    ss << request.prompt << "|";
    ss << request.original_code << "|";
    ss << static_cast<int>(request.pattern) << "|";
    
    // Include backend names or pipeline name
    if (!request.backend_names.empty()) {
        for (const auto& backend : request.backend_names) {
            ss << backend << ",";
        }
    } else {
        ss << request.pipeline_name;
    }
    
    // Simple hash function
    std::hash<std::string> hasher;
    return std::to_string(hasher(ss.str()));
}

double SequentialStrategy::calculateStepQuality(const PipelineStep& step, 
                                              const std::string& output, 
                                              const std::string& context) {
    // Get quality scorer function
    auto scorer = getQualityScorer(context);
    
    // Base quality from scorer
    double quality = scorer(output);
    
    // Adjust based on step-specific criteria
    if (!output.empty()) {
        // Length-based quality component
        size_t optimal_length = 500; // Reasonable default
        if (step.step_parameters.find("optimal_length") != step.step_parameters.end()) {
            optimal_length = std::stoul(step.step_parameters.at("optimal_length"));
        }
        
        double length_ratio = static_cast<double>(output.length()) / optimal_length;
        double length_score = 1.0 - std::abs(1.0 - length_ratio) * 0.3;
        length_score = std::max(0.0, std::min(1.0, length_score));
        
        quality = quality * 0.7 + length_score * 0.3;
    }
    
    return std::max(0.0, std::min(1.0, quality));
}

void SequentialStrategy::updateStatistics(const SequentialRequest& request, 
                                         const SequentialResponse& response) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_statistics.total_pipelines++;
    
    if (response.success) {
        m_statistics.successful_pipelines++;
    } else {
        m_statistics.failed_pipelines++;
    }
    
    if (response.rollback_occurred) {
        m_statistics.rollback_count++;
    }
    
    // Update averages
    double n = static_cast<double>(m_statistics.total_pipelines);
    m_statistics.average_pipeline_time = 
        (m_statistics.average_pipeline_time * (n - 1) + response.total_time.count()) / n;
    m_statistics.average_coordination_time = 
        (m_statistics.average_coordination_time * (n - 1) + response.coordination_time.count()) / n;
    m_statistics.average_quality_score = 
        (m_statistics.average_quality_score * (n - 1) + response.overall_quality_score) / n;
    m_statistics.average_steps_per_pipeline = 
        (m_statistics.average_steps_per_pipeline * (n - 1) + response.steps_completed) / n;
    
    // Update pattern usage
    m_statistics.pattern_usage[response.pattern_used]++;
    
    // Update model usage
    for (const auto& step : response.steps_executed) {
        m_statistics.model_step_usage[step.model_name]++;
    }
    
    // Update step success rates
    for (const auto& [step_id, quality] : response.step_quality_scores) {
        auto& success_rate = m_statistics.step_success_rates[step_id];
        success_rate = (success_rate * (n - 1) + (quality > 0 ? 1.0 : 0.0)) / n;
    }
    
    // Update task quality scores
    if (request.task_type != TaskType::UNKNOWN) {
        auto& task_quality = m_statistics.task_quality_scores[request.task_type];
        size_t task_count = 0;
        for (const auto& [pattern, count] : m_statistics.pattern_usage) {
            task_count += count;
        }
        task_quality = (task_quality * (task_count - 1) + response.overall_quality_score) / task_count;
    }
}

SequentialStrategyConfig SequentialStrategy::getConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

void SequentialStrategy::setConfig(const SequentialStrategyConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
}

SequentialStatistics SequentialStrategy::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_statistics;
}

void SequentialStrategy::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_statistics = SequentialStatistics();
    m_statistics.last_reset = std::chrono::system_clock::now();
}

void SequentialStrategy::registerPatternTemplate(SequentialPattern pattern, 
                                                const std::vector<PipelineStep>& steps) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.pattern_templates[pattern] = steps;
}

void SequentialStrategy::registerNamedPipeline(const std::string& name, 
                                              const std::vector<std::string>& backend_names) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config.named_pipelines[name] = backend_names;
}

void SequentialStrategy::clearCache() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    m_cache.clear();
}

std::unordered_map<std::string, double> SequentialStrategy::getCacheStatistics() const {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    
    std::unordered_map<std::string, double> stats;
    stats["cache_size"] = static_cast<double>(m_cache.size());
    stats["max_cache_size"] = static_cast<double>(m_config.max_cache_size);
    
    if (!m_cache.empty()) {
        double total_accesses = 0;
        double total_quality = 0;
        
        for (const auto& [key, cached] : m_cache) {
            total_accesses += cached.access_count;
            total_quality += cached.quality_score;
        }
        
        stats["average_accesses_per_entry"] = total_accesses / m_cache.size();
        stats["average_cached_quality"] = total_quality / m_cache.size();
        stats["cache_hit_rate"] = total_accesses / (m_statistics.total_pipelines + 1.0);
    }
    
    return stats;
}

void SequentialStrategy::initializeDefaultTemplates() {
    // Initialize default pattern templates if not already configured
    if (m_config.pattern_templates.empty()) {
        // These are initialized through getStepsForPattern() method
        // which provides default implementations for each pattern
    }
}

void SequentialStrategy::initializeDefaultPrompts() {
    // Initialize default prompt templates
    m_config.prompt_templates["refine"] = 
        "Please refine and improve the following code:\n\n{{{current_output}}}";
    
    m_config.prompt_templates["analyze"] = 
        "Analyze the following code and provide insights:\n\n{{{current_output}}}";
    
    m_config.prompt_templates["validate"] = 
        "Validate the following code for correctness and best practices:\n\n{{{current_output}}}";
    
    m_config.prompt_templates["optimize"] = 
        "Optimize the following code for better performance:\n\n{{{current_output}}}";
    
    m_config.prompt_templates["document"] = 
        "Add comprehensive documentation to the following code:\n\n{{{current_output}}}";
}

void SequentialStrategy::logOriginalFormatOutput(size_t step_number, size_t total_steps, 
                                                const std::string& model_name, 
                                                SequentialResponse& response) {
    std::string log_message = "[INFO] Pipeline Step " + std::to_string(step_number) + 
                             "/" + std::to_string(total_steps) + 
                             ": Using model '" + model_name + "'...";
    
    response.pipeline_log.push_back(log_message);
    
    if (m_config.enable_console_output) {
        Logger::getInstance().info("SequentialStrategy", log_message);
    }
}

void SequentialStrategy::cleanupCache() {
    // Remove expired entries
    auto now = std::chrono::system_clock::now();
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        auto age = now - it->second.created_at;
        if (age > m_config.cache_ttl) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

std::string SequentialStrategy::applyTemplate(const std::string& template_str, 
                                             const std::unordered_map<std::string, std::string>& variables) {
    std::string result = template_str;
    
    // Replace all {{{variable}}} placeholders
    std::regex var_regex(R"(\{\{\{(\w+)\}\}\})");
    std::smatch match;
    
    while (std::regex_search(result, match, var_regex)) {
        std::string var_name = match[1];
        auto it = variables.find(var_name);
        
        if (it != variables.end()) {
            result.replace(match.position(), match.length(), it->second);
        } else {
            // Leave placeholder if variable not found
            result.replace(match.position(), match.length(), "");
        }
    }
    
    return result;
}

double SequentialStrategy::calculateOverallQuality(const PipelineState& state) {
    if (state.quality_scores.empty()) {
        return 0.0;
    }
    
    // Calculate weighted average of step qualities
    double total_quality = 0.0;
    double total_weight = 0.0;
    
    for (const auto& [step_id, quality] : state.quality_scores) {
        // Later steps get slightly higher weight
        double weight = 1.0 + (0.1 * std::count_if(state.processing_history.begin(), 
                                                   state.processing_history.end(),
                                                   [&step_id](const std::string& entry) {
                                                       return entry.find(step_id) != std::string::npos;
                                                   }));
        
        total_quality += quality * weight;
        total_weight += weight;
    }
    
    return total_weight > 0 ? (total_quality / total_weight) : 0.0;
}

std::function<double(const std::string&)> SequentialStrategy::getQualityScorer(
    const std::string& context) {
    
    return [context](const std::string& output) -> double {
        double score = 0.5; // Base score
        
        // Check for common quality indicators
        if (!output.empty()) {
            // Length-based scoring
            if (output.length() > 100) score += 0.1;
            if (output.length() > 500) score += 0.1;
            
            // Content-based scoring
            if (output.find("```") != std::string::npos) score += 0.15; // Contains code blocks
            if (output.find("error") == std::string::npos && 
                output.find("Error") == std::string::npos) score += 0.1; // No errors
            
            // Structure indicators
            if (output.find("\n") != std::string::npos) score += 0.05; // Multi-line
            
            // Context-specific scoring
            if (!context.empty()) {
                // Check if output addresses the context
                std::istringstream iss(context);
                std::string word;
                int context_matches = 0;
                int total_words = 0;
                
                while (iss >> word && total_words < 10) {
                    if (word.length() > 3 && output.find(word) != std::string::npos) {
                        context_matches++;
                    }
                    total_words++;
                }
                
                if (total_words > 0) {
                    score += 0.2 * (static_cast<double>(context_matches) / total_words);
                }
            }
        }
        
        return std::max(0.0, std::min(1.0, score));
    };
}

// Utility functions

std::string sequentialPatternToString(SequentialPattern pattern) {
    switch (pattern) {
        case SequentialPattern::ORIGINAL_REFINEMENT:
            return "original_refinement";
        case SequentialPattern::ANALYSIS_GENERATION:
            return "analysis_generation";
        case SequentialPattern::GENERATION_VALIDATION:
            return "generation_validation";
        case SequentialPattern::PLANNING_IMPLEMENTATION:
            return "planning_implementation";
        case SequentialPattern::REVIEW_REFINEMENT:
            return "review_refinement";
        case SequentialPattern::CUSTOM:
            return "custom";
        default:
            return "unknown";
    }
}

SequentialPattern stringToSequentialPattern(const std::string& str) {
    if (str == "original_refinement") return SequentialPattern::ORIGINAL_REFINEMENT;
    if (str == "analysis_generation") return SequentialPattern::ANALYSIS_GENERATION;
    if (str == "generation_validation") return SequentialPattern::GENERATION_VALIDATION;
    if (str == "planning_implementation") return SequentialPattern::PLANNING_IMPLEMENTATION;
    if (str == "review_refinement") return SequentialPattern::REVIEW_REFINEMENT;
    if (str == "custom") return SequentialPattern::CUSTOM;
    
    return SequentialPattern::ORIGINAL_REFINEMENT; // Default
}

} // namespace Camus