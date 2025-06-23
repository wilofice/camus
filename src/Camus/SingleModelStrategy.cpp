// =================================================================
// src/Camus/SingleModelStrategy.cpp
// =================================================================
// Implementation of enhanced single-model execution strategy.

#include "Camus/SingleModelStrategy.hpp"
#include "Camus/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <random>
#include <numeric>

namespace Camus {

SingleModelStrategy::SingleModelStrategy(ModelRegistry& registry, 
                                       const SingleModelStrategyConfig& config)
    : m_registry(registry), m_config(config) {
    
    // Initialize default templates and parameters
    initializeDefaultTemplates();
    initializeDefaultParameters();
    
    // Set default quality scorer
    m_quality_scorer = [this](const std::string& prompt, const std::string& response) {
        return defaultQualityScorer(prompt, response);
    };
    
    // Initialize statistics
    m_statistics.last_reset = std::chrono::system_clock::now();
    
    Logger::getInstance().info("SingleModelStrategy", "Strategy initialized successfully");
}

StrategyResponse SingleModelStrategy::execute(const StrategyRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    StrategyResponse response;
    response.request_id = request.request_id;
    
    Logger::getInstance().debug("SingleModelStrategy", 
        "Executing request: " + request.request_id + " with model: " + request.selected_model);
    
    try {
        // Get model instance and metadata
        auto model = m_registry.getModel(request.selected_model);
        if (!model) {
            throw std::runtime_error("Model not available: " + request.selected_model);
        }
        
        auto model_metadata = model->getModelMetadata();
        response.model_used = request.selected_model;
        
        // Step 1: Optimize prompt if enabled
        std::string optimized_prompt = request.prompt;
        auto optimization_start = std::chrono::steady_clock::now();
        
        if (m_config.enable_prompt_optimization && request.enable_optimization) {
            optimized_prompt = optimizePrompt(request, model_metadata);
            response.optimization_steps.push_back("prompt_optimization");
        }
        
        auto optimization_end = std::chrono::steady_clock::now();
        response.optimization_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            optimization_end - optimization_start);
        response.optimized_prompt = optimized_prompt;
        
        // Step 2: Manage context window if enabled
        if (m_config.enable_context_management) {
            optimized_prompt = manageContextWindow(optimized_prompt, request.context, model_metadata);
            response.optimization_steps.push_back("context_management");
        }
        
        // Step 3: Tune parameters if enabled
        StrategyRequest tuned_request = request;
        if (m_config.enable_parameter_tuning) {
            tuned_request = tuneParameters(request, model_metadata);
            response.optimization_steps.push_back("parameter_tuning");
        }
        
        // Step 4: Execute the request
        auto execution_start = std::chrono::steady_clock::now();
        
        // Set up model parameters (this would typically be done through model interface)
        // For now, we'll use the optimized prompt directly
        std::string model_response = model->getCompletion(optimized_prompt);
        
        auto execution_end = std::chrono::steady_clock::now();
        response.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            execution_end - execution_start);
        
        response.response_text = model_response;
        response.tokens_generated = estimateTokenCount(model_response);
        response.context_tokens_used = estimateTokenCount(optimized_prompt);
        
        // Calculate context utilization
        size_t max_context = model_metadata.performance.max_context_tokens;
        if (max_context > 0) {
            response.context_utilization = static_cast<double>(response.context_tokens_used) / max_context;
        }
        
        // Step 5: Validate response if enabled
        auto validation_start = std::chrono::steady_clock::now();
        
        if (m_config.enable_response_validation && request.enable_validation) {
            response.quality_score = validateResponse(tuned_request, model_response, model_metadata);
            response.optimization_steps.push_back("response_validation");
        } else {
            response.quality_score = 0.8; // Default quality score
        }
        
        auto validation_end = std::chrono::steady_clock::now();
        response.validation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            validation_end - validation_start);
        
        response.success = true;
        
        // Add debug information
        response.debug_info["model_capabilities"] = std::to_string(model_metadata.capabilities.size());
        response.debug_info["context_utilization"] = std::to_string(response.context_utilization);
        response.debug_info["optimization_steps"] = std::to_string(response.optimization_steps.size());
        
        Logger::getInstance().info("SingleModelStrategy", 
            "Request completed successfully: " + request.request_id + 
            " (quality: " + std::to_string(response.quality_score) + 
            ", time: " + std::to_string(response.execution_time.count()) + "ms)");
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = e.what();
        
        Logger::getInstance().error("SingleModelStrategy", 
            "Request execution failed: " + response.error_message + 
            " (request: " + request.request_id + ")");
    }
    
    // Calculate total time
    auto end_time = std::chrono::steady_clock::now();
    response.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    // Update statistics and adapt parameters if enabled
    updateStatistics(request, response);
    
    if (m_config.parameter_config.enable_adaptive_parameters && response.success) {
        adaptParameters(request.selected_model, request.task_type, 
                       response.quality_score, response.execution_time);
    }
    
    return response;
}

std::string SingleModelStrategy::optimizePrompt(const StrategyRequest& request, 
                                               const ModelMetadata& model_metadata) {
    
    std::string optimized = request.prompt;
    
    // Get appropriate prompt template
    PromptTemplate template_obj = getPromptTemplate(model_metadata, request.task_type);
    
    // Apply template formatting
    optimized = applyPromptTemplate(template_obj, request.prompt, request.context);
    
    // Apply model-specific optimizations
    optimized = applyModelSpecificOptimizations(optimized, model_metadata, request.task_type);
    
    Logger::getInstance().debug("SingleModelStrategy", 
        "Prompt optimized for model: " + model_metadata.name + 
        " (original: " + std::to_string(request.prompt.length()) + 
        " chars, optimized: " + std::to_string(optimized.length()) + " chars)");
    
    return optimized;
}

std::string SingleModelStrategy::manageContextWindow(const std::string& prompt,
                                                    const std::string& context,
                                                    const ModelMetadata& model_metadata) {
    
    size_t max_context = model_metadata.performance.max_context_tokens;
    size_t reserved_output = m_config.context_config.reserved_output_tokens;
    size_t available_tokens = max_context > reserved_output ? max_context - reserved_output : max_context / 2;
    
    std::string combined = prompt;
    if (!context.empty()) {
        combined += "\n\n" + context;
    }
    
    size_t current_tokens = estimateTokenCount(combined);
    size_t target_tokens = static_cast<size_t>(available_tokens * m_config.context_config.utilization_target);
    
    if (current_tokens <= target_tokens) {
        return combined; // No truncation needed
    }
    
    if (!m_config.context_config.enable_truncation) {
        return combined; // Truncation disabled
    }
    
    // Truncate based on strategy
    std::string truncated = truncateContext(combined, target_tokens, 
                                          m_config.context_config.truncation_strategy);
    
    Logger::getInstance().debug("SingleModelStrategy", 
        "Context managed: " + std::to_string(current_tokens) + " -> " + 
        std::to_string(estimateTokenCount(truncated)) + " tokens");
    
    return truncated;
}

StrategyRequest SingleModelStrategy::tuneParameters(const StrategyRequest& request,
                                                   const ModelMetadata& model_metadata) {
    
    StrategyRequest tuned = request;
    
    // Apply capability-based adjustments
    for (const auto& capability : model_metadata.capabilities) {
        auto temp_it = m_config.parameter_config.temperature_adjustments.find(capability);
        if (temp_it != m_config.parameter_config.temperature_adjustments.end()) {
            tuned.temperature += temp_it->second;
        }
        
        auto top_p_it = m_config.parameter_config.top_p_adjustments.find(capability);
        if (top_p_it != m_config.parameter_config.top_p_adjustments.end()) {
            tuned.top_p += top_p_it->second;
        }
        
        auto top_k_it = m_config.parameter_config.top_k_adjustments.find(capability);
        if (top_k_it != m_config.parameter_config.top_k_adjustments.end()) {
            tuned.top_k += top_k_it->second;
        }
    }
    
    // Apply task-based adjustments
    auto task_temp_it = m_config.parameter_config.task_temperature_adjustments.find(request.task_type);
    if (task_temp_it != m_config.parameter_config.task_temperature_adjustments.end()) {
        tuned.temperature += task_temp_it->second;
    }
    
    auto task_tokens_it = m_config.parameter_config.task_max_tokens_adjustments.find(request.task_type);
    if (task_tokens_it != m_config.parameter_config.task_max_tokens_adjustments.end()) {
        tuned.max_tokens += task_tokens_it->second;
    }
    
    // Clamp values to valid ranges
    tuned.temperature = std::max(0.1, std::min(2.0, tuned.temperature));
    tuned.top_p = std::max(0.1, std::min(1.0, tuned.top_p));
    tuned.top_k = std::max(1, std::min(100, tuned.top_k));
    tuned.max_tokens = std::max(1, std::min(static_cast<int>(model_metadata.performance.max_output_tokens), tuned.max_tokens));
    
    Logger::getInstance().debug("SingleModelStrategy", 
        "Parameters tuned: temp=" + std::to_string(tuned.temperature) + 
        ", top_p=" + std::to_string(tuned.top_p) + 
        ", top_k=" + std::to_string(tuned.top_k) + 
        ", max_tokens=" + std::to_string(tuned.max_tokens));
    
    return tuned;
}

double SingleModelStrategy::validateResponse(const StrategyRequest& request,
                                            const std::string& response,
                                            const ModelMetadata& model_metadata) {
    
    double quality_score = m_quality_scorer(request.prompt, response);
    
    // Apply model-specific quality adjustments
    if (hasCapability(model_metadata, ModelCapability::HIGH_QUALITY)) {
        quality_score += 0.1; // Bonus for high-quality models
    }
    
    if (hasCapability(model_metadata, ModelCapability::CODE_SPECIALIZED) && 
        request.task_type == TaskType::CODE_GENERATION) {
        quality_score += 0.15; // Bonus for code-specialized models on code tasks
    }
    
    // Check for task-specific quality indicators
    if (request.task_type == TaskType::CODE_GENERATION || request.task_type == TaskType::REFACTORING) {
        // Look for code blocks
        if (response.find("```") != std::string::npos || response.find("    ") != std::string::npos) {
            quality_score += 0.1;
        }
    }
    
    // Check response length appropriateness
    double length_ratio = static_cast<double>(response.length()) / request.prompt.length();
    if (length_ratio >= 0.5 && length_ratio <= 10.0) {
        quality_score += 0.05;
    }
    
    // Ensure score is in valid range
    quality_score = std::max(0.0, std::min(1.0, quality_score));
    
    Logger::getInstance().debug("SingleModelStrategy", 
        "Response validated with quality score: " + std::to_string(quality_score));
    
    return quality_score;
}

std::string SingleModelStrategy::applyModelSpecificOptimizations(const std::string& prompt,
                                                                const ModelMetadata& model_metadata,
                                                                TaskType task_type) {
    
    std::string optimized = prompt;
    
    // Code model optimizations
    if (hasCapability(model_metadata, ModelCapability::CODE_SPECIALIZED)) {
        if (task_type == TaskType::CODE_GENERATION) {
            optimized = "Generate clean, well-commented code for the following requirement:\n\n" + optimized;
            optimized += "\n\nPlease provide the code with proper formatting and include explanatory comments.";
        } else if (task_type == TaskType::REFACTORING) {
            optimized = "Refactor the following code to improve its structure, readability, and efficiency:\n\n" + optimized;
            optimized += "\n\nProvide the refactored code with explanations of the changes made.";
        } else if (task_type == TaskType::BUG_FIXING) {
            optimized = "Analyze the following code for bugs and provide a corrected version:\n\n" + optimized;
            optimized += "\n\nExplain what bugs were found and how they were fixed.";
        }
    }
    
    // Analysis model optimizations
    if (hasCapability(model_metadata, ModelCapability::REASONING)) {
        if (task_type == TaskType::CODE_ANALYSIS) {
            optimized = "Provide a detailed analysis of the following code:\n\n" + optimized;
            optimized += "\n\nInclude information about structure, complexity, potential issues, and suggestions for improvement.";
        } else if (task_type == TaskType::SECURITY_REVIEW) {
            optimized = "Perform a security analysis of the following code:\n\n" + optimized;
            optimized += "\n\nIdentify potential security vulnerabilities and provide recommendations for mitigation.";
        }
    }
    
    // Fast model optimizations (simplified prompts)
    if (hasCapability(model_metadata, ModelCapability::FAST_INFERENCE)) {
        if (task_type == TaskType::SIMPLE_QUERY) {
            // Keep prompt concise for fast models
            optimized = std::regex_replace(optimized, std::regex("Please provide.*detailed.*"), "");
            optimized = std::regex_replace(optimized, std::regex("Explain in detail"), "Explain");
        }
    }
    
    // High-quality model optimizations
    if (hasCapability(model_metadata, ModelCapability::HIGH_QUALITY)) {
        optimized += "\n\nPlease provide a comprehensive and detailed response.";
    }
    
    // Chat-optimized model formatting
    if (hasCapability(model_metadata, ModelCapability::CHAT_OPTIMIZED)) {
        if (!optimized.empty() && optimized[0] != '[' && optimized.find("Human:") == std::string::npos) {
            optimized = "Human: " + optimized + "\n\nAssistant:";
        }
    }
    
    Logger::getInstance().debug("SingleModelStrategy", 
        "Applied model-specific optimizations for capabilities count: " + 
        std::to_string(model_metadata.capabilities.size()));
    
    return optimized;
}

std::string SingleModelStrategy::truncateContext(const std::string& text,
                                                size_t max_tokens,
                                                const std::string& strategy) {
    
    size_t current_tokens = estimateTokenCount(text);
    if (current_tokens <= max_tokens) {
        return text;
    }
    
    // Rough token-to-character ratio (1 token ≈ 4 characters)
    size_t max_chars = max_tokens * 4;
    
    if (strategy == "head") {
        // Keep beginning of text
        return text.substr(0, max_chars);
    } else if (strategy == "tail") {
        // Keep end of text
        size_t start_pos = text.length() > max_chars ? text.length() - max_chars : 0;
        return text.substr(start_pos);
    } else if (strategy == "middle") {
        // Keep beginning and end, remove middle
        size_t half_chars = max_chars / 2;
        std::string beginning = text.substr(0, half_chars);
        std::string ending = text.substr(text.length() - half_chars);
        return beginning + "\n\n[... content truncated ...]\n\n" + ending;
    } else if (strategy == "smart") {
        // Intelligent truncation preserving important sections
        std::vector<std::string> lines;
        std::stringstream ss(text);
        std::string line;
        
        while (std::getline(ss, line)) {
            lines.push_back(line);
        }
        
        // Prioritize lines with code, functions, classes, etc.
        std::vector<std::pair<std::string, int>> prioritized_lines;
        for (const auto& l : lines) {
            int priority = 0;
            if (l.find("function") != std::string::npos || l.find("def ") != std::string::npos) priority += 10;
            if (l.find("class") != std::string::npos) priority += 10;
            if (l.find("import") != std::string::npos || l.find("#include") != std::string::npos) priority += 5;
            if (l.find("//") != std::string::npos || l.find("#") != std::string::npos) priority += 2;
            prioritized_lines.push_back({l, priority});
        }
        
        // Sort by priority and select top lines that fit within limit
        std::sort(prioritized_lines.begin(), prioritized_lines.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::string result;
        size_t current_size = 0;
        for (const auto& pl : prioritized_lines) {
            if (current_size + pl.first.length() + 1 <= max_chars) {
                if (!result.empty()) result += "\n";
                result += pl.first;
                current_size += pl.first.length() + 1;
            }
        }
        
        return result.empty() ? text.substr(0, max_chars) : result;
    }
    
    // Default to tail strategy
    size_t start_pos = text.length() > max_chars ? text.length() - max_chars : 0;
    return text.substr(start_pos);
}

size_t SingleModelStrategy::estimateTokenCount(const std::string& text) {
    // Simple token estimation: ~4 characters per token
    // This could be improved with a proper tokenizer
    return (text.length() + 3) / 4; // Round up
}

void SingleModelStrategy::updateStatistics(const StrategyRequest& request, 
                                          const StrategyResponse& response) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_statistics.total_requests++;
    
    if (response.success) {
        m_statistics.successful_requests++;
    } else {
        m_statistics.failed_requests++;
    }
    
    if (!response.optimization_steps.empty()) {
        m_statistics.optimized_requests++;
    }
    
    // Update averages
    if (m_statistics.total_requests == 1) {
        m_statistics.average_execution_time = response.execution_time.count();
        m_statistics.average_optimization_time = response.optimization_time.count();
        m_statistics.average_quality_score = response.quality_score;
        m_statistics.average_context_utilization = response.context_utilization;
    } else {
        double n = static_cast<double>(m_statistics.total_requests);
        m_statistics.average_execution_time = 
            (m_statistics.average_execution_time * (n - 1) + response.execution_time.count()) / n;
        m_statistics.average_optimization_time = 
            (m_statistics.average_optimization_time * (n - 1) + response.optimization_time.count()) / n;
        m_statistics.average_quality_score = 
            (m_statistics.average_quality_score * (n - 1) + response.quality_score) / n;
        m_statistics.average_context_utilization = 
            (m_statistics.average_context_utilization * (n - 1) + response.context_utilization) / n;
    }
    
    // Update usage counts
    m_statistics.model_usage[response.model_used]++;
    m_statistics.task_distribution[request.task_type]++;
    
    // Update optimization effectiveness
    if (!response.optimization_steps.empty()) {
        for (const auto& step : response.optimization_steps) {
            if (m_statistics.optimization_effectiveness.find(step) == m_statistics.optimization_effectiveness.end()) {
                m_statistics.optimization_effectiveness[step] = response.quality_score;
            } else {
                m_statistics.optimization_effectiveness[step] = 
                    (m_statistics.optimization_effectiveness[step] + response.quality_score) / 2.0;
            }
        }
    }
}

void SingleModelStrategy::adaptParameters(const std::string& model_name, 
                                         TaskType task_type,
                                         double quality_score, 
                                         std::chrono::milliseconds execution_time) {
    
    std::lock_guard<std::mutex> lock(m_config_mutex);
    
    double learning_rate = m_config.parameter_config.adaptation_learning_rate;
    
    // Adapt temperature based on quality score
    auto& task_temps = m_config.parameter_config.task_temperature_adjustments;
    double current_temp = task_temps[task_type];
    
    if (quality_score < 0.6) {
        // Low quality - try adjusting temperature
        if (current_temp > 0.7) {
            task_temps[task_type] = current_temp - learning_rate * 0.1; // Reduce temperature
        } else {
            task_temps[task_type] = current_temp + learning_rate * 0.1; // Increase temperature
        }
    } else if (quality_score > 0.8) {
        // High quality - slight adjustment towards current direction
        if (execution_time.count() > 5000) { // If too slow
            task_temps[task_type] = current_temp + learning_rate * 0.05; // Slightly increase for speed
        }
    }
    
    // Adapt max tokens based on execution time
    auto& task_tokens = m_config.parameter_config.task_max_tokens_adjustments;
    if (execution_time.count() > 10000 && quality_score > 0.7) {
        // Too slow but good quality - try reducing tokens
        task_tokens[task_type] = std::max(-512, task_tokens[task_type] - 64);
    } else if (execution_time.count() < 2000 && quality_score < 0.6) {
        // Fast but poor quality - try increasing tokens
        task_tokens[task_type] = std::min(512, task_tokens[task_type] + 64);
    }
    
    Logger::getInstance().debug("SingleModelStrategy", 
        "Adapted parameters for " + model_name + " task " + std::to_string(static_cast<int>(task_type)) +
        ": quality=" + std::to_string(quality_score) + 
        ", time=" + std::to_string(execution_time.count()) + "ms");
}

SingleModelStrategyConfig SingleModelStrategy::getConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

void SingleModelStrategy::setConfig(const SingleModelStrategyConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
    Logger::getInstance().info("SingleModelStrategy", "Configuration updated");
}

SingleModelStatistics SingleModelStrategy::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_statistics;
}

void SingleModelStrategy::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_statistics = SingleModelStatistics();
    m_statistics.last_reset = std::chrono::system_clock::now();
    Logger::getInstance().info("SingleModelStrategy", "Statistics reset");
}

void SingleModelStrategy::registerPromptTemplate(const std::string& model_name, 
                                                const PromptTemplate& template_obj) {
    m_model_templates[model_name] = template_obj;
    Logger::getInstance().info("SingleModelStrategy", 
        "Registered prompt template for: " + model_name);
}

void SingleModelStrategy::registerQualityScorer(std::function<double(const std::string&, const std::string&)> scorer) {
    m_quality_scorer = scorer;
    Logger::getInstance().info("SingleModelStrategy", "Custom quality scorer registered");
}

void SingleModelStrategy::initializeDefaultTemplates() {
    // Code model template
    PromptTemplate code_template;
    code_template.template_name = "code_specialized";
    code_template.system_prompt = "You are an expert programming assistant. Provide clean, efficient, and well-documented code.";
    code_template.user_prompt_format = "{prompt}";
    code_template.context_format = "\n\nContext:\n{context}";
    code_template.task_specific_instructions[TaskType::CODE_GENERATION] = 
        "\n\nPlease provide complete, working code with comments explaining the logic.";
    code_template.task_specific_instructions[TaskType::REFACTORING] = 
        "\n\nRefactor this code to improve readability, efficiency, and maintainability.";
    code_template.task_specific_instructions[TaskType::BUG_FIXING] = 
        "\n\nIdentify and fix any bugs in this code, explaining what was wrong.";
    code_template.target_capabilities = {ModelCapability::CODE_SPECIALIZED};
    code_template.temperature_adjustment = -0.1; // Slightly more deterministic for code
    
    m_model_templates["code_specialized"] = code_template;
    
    // Analysis model template
    PromptTemplate analysis_template;
    analysis_template.template_name = "analysis_focused";
    analysis_template.system_prompt = "You are a detailed code analysis expert. Provide thorough, structured analysis.";
    analysis_template.user_prompt_format = "{prompt}";
    analysis_template.context_format = "\n\nCode to analyze:\n{context}";
    analysis_template.task_specific_instructions[TaskType::CODE_ANALYSIS] = 
        "\n\nProvide a comprehensive analysis including structure, complexity, and improvement suggestions.";
    analysis_template.task_specific_instructions[TaskType::SECURITY_REVIEW] = 
        "\n\nFocus on security vulnerabilities and provide specific mitigation strategies.";
    analysis_template.target_capabilities = {ModelCapability::REASONING, ModelCapability::SECURITY_FOCUSED};
    
    m_model_templates["analysis_focused"] = analysis_template;
    
    // Fast model template
    PromptTemplate fast_template;
    fast_template.template_name = "fast_inference";
    fast_template.system_prompt = "Provide concise, direct responses.";
    fast_template.user_prompt_format = "{prompt}";
    fast_template.context_format = "\n{context}";
    fast_template.task_specific_instructions[TaskType::SIMPLE_QUERY] = 
        "\n\nProvide a brief, direct answer.";
    fast_template.target_capabilities = {ModelCapability::FAST_INFERENCE};
    fast_template.temperature_adjustment = 0.1; // Slightly more creative for quick responses
    fast_template.max_tokens_adjustment = -256; // Shorter responses
    
    m_model_templates["fast_inference"] = fast_template;
    
    Logger::getInstance().info("SingleModelStrategy", "Initialized default prompt templates");
}

void SingleModelStrategy::initializeDefaultParameters() {
    // Capability-based temperature adjustments
    m_config.parameter_config.temperature_adjustments[ModelCapability::CODE_SPECIALIZED] = -0.1;
    m_config.parameter_config.temperature_adjustments[ModelCapability::FAST_INFERENCE] = 0.1;
    m_config.parameter_config.temperature_adjustments[ModelCapability::HIGH_QUALITY] = -0.05;
    m_config.parameter_config.temperature_adjustments[ModelCapability::CREATIVE_WRITING] = 0.2;
    
    // Task-based temperature adjustments
    m_config.parameter_config.task_temperature_adjustments[TaskType::CODE_GENERATION] = -0.2;
    m_config.parameter_config.task_temperature_adjustments[TaskType::REFACTORING] = -0.15;
    m_config.parameter_config.task_temperature_adjustments[TaskType::BUG_FIXING] = -0.25;
    m_config.parameter_config.task_temperature_adjustments[TaskType::DOCUMENTATION] = 0.1;
    m_config.parameter_config.task_temperature_adjustments[TaskType::SIMPLE_QUERY] = 0.05;
    
    // Task-based token adjustments
    m_config.parameter_config.task_max_tokens_adjustments[TaskType::CODE_GENERATION] = 512;
    m_config.parameter_config.task_max_tokens_adjustments[TaskType::CODE_ANALYSIS] = 256;
    m_config.parameter_config.task_max_tokens_adjustments[TaskType::SIMPLE_QUERY] = -256;
    m_config.parameter_config.task_max_tokens_adjustments[TaskType::DOCUMENTATION] = 384;
    
    Logger::getInstance().info("SingleModelStrategy", "Initialized default parameter configurations");
}

PromptTemplate SingleModelStrategy::getPromptTemplate(const ModelMetadata& model_metadata, 
                                                     TaskType task_type) {
    
    // First check for exact model name match
    auto model_it = m_model_templates.find(model_metadata.name);
    if (model_it != m_model_templates.end()) {
        return model_it->second;
    }
    
    // Check for capability-based templates
    for (const auto& capability : model_metadata.capabilities) {
        if (capability == ModelCapability::CODE_SPECIALIZED) {
            auto it = m_model_templates.find("code_specialized");
            if (it != m_model_templates.end()) return it->second;
        } else if (capability == ModelCapability::REASONING || capability == ModelCapability::SECURITY_FOCUSED) {
            auto it = m_model_templates.find("analysis_focused");
            if (it != m_model_templates.end()) return it->second;
        } else if (capability == ModelCapability::FAST_INFERENCE) {
            auto it = m_model_templates.find("fast_inference");
            if (it != m_model_templates.end()) return it->second;
        }
    }
    
    // Check task-specific templates
    auto task_it = m_task_templates.find(task_type);
    if (task_it != m_task_templates.end()) {
        return task_it->second;
    }
    
    // Return default template
    PromptTemplate default_template;
    default_template.template_name = "default";
    default_template.user_prompt_format = "{prompt}";
    default_template.context_format = "\n\n{context}";
    return default_template;
}

std::string SingleModelStrategy::applyPromptTemplate(const PromptTemplate& template_obj,
                                                    const std::string& prompt,
                                                    const std::string& context) {
    
    std::string result;
    
    // Add system prompt if present
    if (!template_obj.system_prompt.empty()) {
        result = template_obj.system_prompt + "\n\n";
    }
    
    // Apply user prompt format
    std::string user_part = template_obj.user_prompt_format;
    user_part = std::regex_replace(user_part, std::regex("\\{prompt\\}"), prompt);
    result += user_part;
    
    // Add context if present
    if (!context.empty() && !template_obj.context_format.empty()) {
        std::string context_part = template_obj.context_format;
        context_part = std::regex_replace(context_part, std::regex("\\{context\\}"), context);
        result += context_part;
    }
    
    return result;
}

double SingleModelStrategy::defaultQualityScorer(const std::string& prompt, const std::string& response) {
    if (response.empty()) {
        return 0.0;
    }
    
    double score = 0.5; // Base score
    
    // Length appropriateness
    double length_ratio = static_cast<double>(response.length()) / prompt.length();
    if (length_ratio >= 0.5 && length_ratio <= 10.0) {
        score += 0.15;
    }
    
    // Check for code indicators if relevant
    if (prompt.find("code") != std::string::npos || prompt.find("function") != std::string::npos) {
        if (response.find("```") != std::string::npos || response.find("def ") != std::string::npos ||
            response.find("function") != std::string::npos || response.find("class") != std::string::npos) {
            score += 0.2;
        }
    }
    
    // Check for structure and formatting
    size_t line_count = std::count(response.begin(), response.end(), '\n');
    if (line_count > 2) {
        score += 0.1; // Bonus for structured responses
    }
    
    // Check for error indicators
    std::string lower_response = response;
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(), ::tolower);
    
    if (lower_response.find("error") == std::string::npos &&
        lower_response.find("sorry") == std::string::npos &&
        lower_response.find("cannot") == std::string::npos &&
        lower_response.find("unable") == std::string::npos) {
        score += 0.15;
    }
    
    return std::min(1.0, score);
}

std::vector<std::string> SingleModelStrategy::extractKeywords(const std::string& text) {
    std::vector<std::string> keywords;
    std::regex word_regex(R"(\b\w{3,}\b)"); // Words with 3+ characters
    std::sregex_iterator words_begin(text.begin(), text.end(), word_regex);
    std::sregex_iterator words_end;
    
    for (auto it = words_begin; it != words_end; ++it) {
        std::string word = it->str();
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        keywords.push_back(word);
    }
    
    return keywords;
}

bool SingleModelStrategy::hasCapability(const ModelMetadata& model_metadata, ModelCapability capability) {
    return std::find(model_metadata.capabilities.begin(), model_metadata.capabilities.end(), capability) 
           != model_metadata.capabilities.end();
}

} // namespace Camus