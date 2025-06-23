// =================================================================
// src/Camus/ModelRegistry.cpp
// =================================================================
// Implementation of centralized model registry.

#include "Camus/ModelRegistry.hpp"
#include "Camus/LlamaCppInteraction.hpp"
#include "Camus/OllamaInteraction.hpp"
#include "Camus/Logger.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <regex>
#include <condition_variable>
#include <atomic>

namespace Camus {

ModelRegistry::ModelRegistry(const RegistryConfig& config) 
    : m_config(config), m_model_pool(std::make_unique<ConcreteModelPool>()) {
    
    // Register default model factories
    registerModelFactory("llama_cpp", [this](const ModelConfig& cfg) {
        return createLlamaCppModel(cfg);
    });
    
    registerModelFactory("ollama", [this](const ModelConfig& cfg) {
        return createOllamaModel(cfg);
    });
    
    m_status.last_update = std::chrono::system_clock::now();
    
    // Auto-discover models if enabled
    if (m_config.auto_discover && !m_config.config_file_path.empty()) {
        loadFromConfig(m_config.config_file_path);
    }
    
    // Start health check thread if enabled
    if (m_config.enable_health_checks) {
        startHealthCheckThread();
    }
}

ModelRegistry::~ModelRegistry() {
    stopHealthCheckThread();
    m_model_pool->cleanupAll();
}

RegistryStatus ModelRegistry::loadFromConfig(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    
    Logger::getInstance().info("ModelRegistry", "Loading models from configuration: " + config_path);
    
    m_status.load_results.clear();
    
    try {
        // Parse configuration file
        auto configs = parseConfigFile(config_path);
        m_status.total_configured = configs.size();
        
        // Clear existing configs and reload
        m_model_configs.clear();
        
        for (const auto& config : configs) {
            m_model_configs[config.name] = config;
            
            // Validate configuration if enabled
            if (m_config.validate_on_load && !validateModelConfig(config)) {
                ModelLoadResult result;
                result.success = false;
                result.model_id = config.name;
                result.error_message = "Invalid configuration";
                m_status.load_results.push_back(result);
                m_status.failed_to_load++;
                continue;
            }
            
            // Load the model
            auto result = loadModel(config);
            m_status.load_results.push_back(result);
            
            if (result.success) {
                m_status.successfully_loaded++;
            } else {
                m_status.failed_to_load++;
            }
        }
        
        // Update status
        updateStatus();
        
        Logger::getInstance().info("ModelRegistry",
            "Model loading complete. Loaded: " + std::to_string(m_status.successfully_loaded) +
            "/" + std::to_string(m_status.total_configured));
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("ModelRegistry", 
            "Failed to load configuration: " + std::string(e.what()));
    }
    
    return m_status;
}

RegistryStatus ModelRegistry::reloadConfiguration() {
    if (m_config.config_file_path.empty()) {
        Logger::getInstance().warning("ModelRegistry", "No configuration file path set");
        return m_status;
    }
    
    return loadFromConfig(m_config.config_file_path);
}

void ModelRegistry::registerModelFactory(const std::string& model_type, ModelFactory factory) {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    m_factories[model_type] = factory;
    Logger::getInstance().info("ModelRegistry", "Registered factory for model type: " + model_type);
}

std::shared_ptr<LlmInteraction> ModelRegistry::getModel(const std::string& model_name) {
    return m_model_pool->getModel(model_name);
}

ModelPool& ModelRegistry::getModelPool() {
    return *m_model_pool;
}

RegistryStatus ModelRegistry::getStatus() const {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    return m_status;
}

std::vector<ModelConfig> ModelRegistry::getConfiguredModels() const {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    std::vector<ModelConfig> configs;
    configs.reserve(m_model_configs.size());
    
    for (const auto& [name, config] : m_model_configs) {
        configs.push_back(config);
    }
    
    return configs;
}

std::vector<std::string> ModelRegistry::getLoadedModels() const {
    return m_model_pool->getAllModelIds();
}

size_t ModelRegistry::performHealthChecks() {
    Logger::getInstance().info("ModelRegistry", "Performing health checks on all models");
    size_t healthy_count = m_model_pool->performHealthChecks();
    
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    m_status.currently_healthy = healthy_count;
    m_status.last_update = std::chrono::system_clock::now();
    
    return healthy_count;
}

bool ModelRegistry::validateModelConfig(const ModelConfig& config) const {
    // Basic validation
    if (config.name.empty()) {
        Logger::getInstance().error("ModelRegistry", "Model configuration missing name");
        return false;
    }
    
    if (config.type.empty()) {
        Logger::getInstance().error("ModelRegistry", "Model configuration missing type");
        return false;
    }
    
    // Type-specific validation
    if (config.type == "llama_cpp") {
        if (config.path.empty()) {
            Logger::getInstance().error("ModelRegistry", 
                "Llama.cpp model missing path: " + config.name);
            return false;
        }
    } else if (config.type == "ollama") {
        if (config.server_url.empty() || config.model_name.empty()) {
            Logger::getInstance().error("ModelRegistry", 
                "Ollama model missing server_url or model_name: " + config.name);
            return false;
        }
    }
    
    // Validate capabilities
    for (const auto& cap_str : config.capabilities) {
        try {
            ModelCapabilityUtils::stringToCapability(cap_str);
        } catch (const std::exception&) {
            Logger::getInstance().error("ModelRegistry", 
                "Invalid capability '" + cap_str + "' in model: " + config.name);
            return false;
        }
    }
    
    return true;
}

ModelLoadResult ModelRegistry::loadModel(const ModelConfig& config) {
    ModelLoadResult result;
    result.model_id = config.name;
    auto start_time = std::chrono::steady_clock::now();
    
    Logger::getInstance().info("ModelRegistry", "Loading model: " + config.name);
    
    // Find appropriate factory
    auto factory_it = m_factories.find(config.type);
    if (factory_it == m_factories.end()) {
        result.error_message = "No factory registered for type: " + config.type;
        Logger::getInstance().error("ModelRegistry", result.error_message);
        return result;
    }
    
    // Attempt to create the model with retries
    size_t attempts = 0;
    while (attempts < m_config.max_load_retries) {
        try {
            auto model = factory_it->second(config);
            if (model) {
                // Add to pool
                if (m_model_pool->addModel(model)) {
                    result.success = true;
                    
                    // Warm up if configured
                    if (m_config.warmup_on_load) {
                        model->warmUp();
                    }
                    
                    auto end_time = std::chrono::steady_clock::now();
                    result.load_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time);
                    
                    Logger::getInstance().info("ModelRegistry", 
                        "Successfully loaded model: " + config.name + 
                        " (took " + std::to_string(result.load_time.count()) + "ms)");
                    break;
                } else {
                    result.error_message = "Failed to add model to pool";
                }
            } else {
                result.error_message = "Factory returned null model";
            }
        } catch (const std::exception& e) {
            result.error_message = e.what();
            Logger::getInstance().error("ModelRegistry", 
                "Failed to load model " + config.name + ": " + e.what());
        }
        
        attempts++;
        if (attempts < m_config.max_load_retries && !result.success) {
            Logger::getInstance().info("ModelRegistry", 
                "Retrying model load (" + std::to_string(attempts) + 
                "/" + std::to_string(m_config.max_load_retries) + ")");
            std::this_thread::sleep_for(m_config.retry_delay);
        }
    }
    
    if (!result.success) {
        Logger::getInstance().error("ModelRegistry", 
            "Failed to load model after " + std::to_string(attempts) + " attempts: " + config.name);
    }
    
    return result;
}

bool ModelRegistry::unloadModel(const std::string& model_name) {
    Logger::getInstance().info("ModelRegistry", "Unloading model: " + model_name);
    
    bool removed = m_model_pool->removeModel(model_name);
    if (removed) {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        updateStatus();
    }
    
    return removed;
}

std::pair<bool, std::chrono::milliseconds> ModelRegistry::testModel(const std::string& model_name) {
    auto model = m_model_pool->getModel(model_name);
    if (!model) {
        return {false, std::chrono::milliseconds(0)};
    }
    
    try {
        auto start_time = std::chrono::steady_clock::now();
        
        InferenceRequest request;
        request.prompt = "Hello, this is a test. Please respond with 'Test successful'.";
        request.max_tokens = 50;
        
        auto response = model->getCompletionWithMetadata(request);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        Logger::getInstance().info("ModelRegistry", 
            "Model test completed for " + model_name + 
            ": " + response.text + " (took " + std::to_string(duration.count()) + "ms)");
        
        return {true, duration};
    } catch (const std::exception& e) {
        Logger::getInstance().error("ModelRegistry", 
            "Model test failed for " + model_name + ": " + e.what());
        return {false, std::chrono::milliseconds(0)};
    }
}

std::string ModelRegistry::getModelInfo(const std::string& model_name) const {
    auto model = m_model_pool->getModel(model_name);
    if (!model) {
        return "Model not found: " + model_name;
    }
    
    std::stringstream ss;
    auto metadata = model->getModelMetadata();
    auto performance = model->getCurrentPerformance();
    
    ss << "Model: " << model_name << "\n";
    ss << "  Description: " << metadata.description << "\n";
    ss << "  Version: " << metadata.version << "\n";
    ss << "  Provider: " << metadata.provider << "\n";
    ss << "  Status: " << (metadata.is_healthy ? "Healthy" : "Unhealthy") << "\n";
    ss << "  Available: " << (metadata.is_available ? "Yes" : "No") << "\n";
    ss << "  Capabilities: ";
    
    auto cap_strings = ModelCapabilityUtils::capabilitiesToStrings(metadata.capabilities);
    for (size_t i = 0; i < cap_strings.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << cap_strings[i];
    }
    ss << "\n";
    
    ss << "  Performance:\n";
    ss << "    Max Context: " << performance.max_context_tokens << " tokens\n";
    ss << "    Max Output: " << performance.max_output_tokens << " tokens\n";
    ss << "    Avg Latency: " << performance.avg_latency.count() << "ms\n";
    ss << "    Tokens/sec: " << performance.tokens_per_second << "\n";
    ss << "    Memory: " << performance.memory_usage_gb << "GB\n";
    
    return ss.str();
}

std::string ModelRegistry::getAllModelsInfo() const {
    std::stringstream ss;
    
    auto status = getStatus();
    ss << "Model Registry Status\n";
    ss << "===================\n";
    ss << "Total Configured: " << status.total_configured << "\n";
    ss << "Successfully Loaded: " << status.successfully_loaded << "\n";
    ss << "Failed to Load: " << status.failed_to_load << "\n";
    ss << "Currently Healthy: " << status.currently_healthy << "\n";
    ss << "\n";
    
    auto model_ids = m_model_pool->getAllModelIds();
    if (model_ids.empty()) {
        ss << "No models loaded.\n";
    } else {
        ss << "Loaded Models:\n";
        ss << "--------------\n";
        for (const auto& model_id : model_ids) {
            ss << "\n" << getModelInfo(model_id);
        }
    }
    
    return ss.str();
}

void ModelRegistry::setConfig(const RegistryConfig& config) {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    
    bool health_check_changed = (m_config.enable_health_checks != config.enable_health_checks);
    m_config = config;
    
    if (health_check_changed) {
        if (m_config.enable_health_checks) {
            startHealthCheckThread();
        } else {
            stopHealthCheckThread();
        }
    }
}

RegistryConfig ModelRegistry::getConfig() const {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    return m_config;
}

std::vector<ModelConfig> ModelRegistry::parseConfigFile(const std::string& config_path) {
    std::vector<ModelConfig> configs;
    
    try {
        YAML::Node root = YAML::LoadFile(config_path);
        
        if (!root["models"]) {
            Logger::getInstance().warning("ModelRegistry", "No 'models' section in configuration file");
            return configs;
        }
        
        YAML::Node models = root["models"];
        for (YAML::const_iterator it = models.begin(); it != models.end(); ++it) {
            ModelConfig config;
            config.name = it->first.as<std::string>();
            
            YAML::Node model_node = it->second;
            
            // Required fields
            if (model_node["type"]) {
                config.type = model_node["type"].as<std::string>();
            }
            
            // Type-specific fields
            if (model_node["path"]) {
                config.path = model_node["path"].as<std::string>();
            }
            if (model_node["server_url"]) {
                config.server_url = model_node["server_url"].as<std::string>();
            }
            if (model_node["model"]) {
                config.model_name = model_node["model"].as<std::string>();
            }
            
            // Optional fields
            if (model_node["description"]) {
                config.description = model_node["description"].as<std::string>();
            }
            if (model_node["version"]) {
                config.version = model_node["version"].as<std::string>();
            }
            
            // Capabilities
            if (model_node["capabilities"]) {
                for (const auto& cap : model_node["capabilities"]) {
                    config.capabilities.push_back(cap.as<std::string>());
                }
            }
            
            // Performance fields
            if (model_node["performance"]) {
                YAML::Node perf = model_node["performance"];
                if (perf["max_context_tokens"]) {
                    config.max_tokens = perf["max_context_tokens"].as<size_t>();
                }
                if (perf["max_output_tokens"]) {
                    config.max_output_tokens = perf["max_output_tokens"].as<size_t>();
                }
                if (perf["memory_usage_gb"]) {
                    config.memory_usage = std::to_string(perf["memory_usage_gb"].as<double>()) + "GB";
                }
                if (perf["expected_tokens_per_second"]) {
                    config.expected_tokens_per_second = perf["expected_tokens_per_second"].as<double>();
                }
                if (perf["expected_latency_ms"]) {
                    config.expected_latency_ms = perf["expected_latency_ms"].as<double>();
                }
            }
            
            // Custom attributes
            if (model_node["custom_attributes"]) {
                for (YAML::const_iterator attr_it = model_node["custom_attributes"].begin();
                     attr_it != model_node["custom_attributes"].end(); ++attr_it) {
                    config.custom_attributes[attr_it->first.as<std::string>()] = 
                        attr_it->second.as<std::string>();
                }
            }
            
            configs.push_back(config);
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("ModelRegistry", 
            "Failed to parse configuration file: " + std::string(e.what()));
    }
    
    return configs;
}

ModelMetadata ModelRegistry::createMetadata(const ModelConfig& config) {
    ModelMetadata metadata;
    
    metadata.name = config.name;
    metadata.description = config.description;
    metadata.version = config.version;
    metadata.provider = config.type;
    metadata.model_path = config.path.empty() ? config.server_url + "/" + config.model_name : config.path;
    
    // Convert capability strings to enums
    metadata.capabilities = ModelCapabilityUtils::parseCapabilities(config.capabilities);
    
    // Set performance characteristics
    metadata.performance.max_context_tokens = config.max_tokens;
    metadata.performance.max_output_tokens = config.max_output_tokens;
    metadata.performance.memory_usage_gb = parseMemoryString(config.memory_usage) / (1024.0 * 1024.0 * 1024.0);
    metadata.performance.tokens_per_second = config.expected_tokens_per_second;
    metadata.performance.avg_latency = std::chrono::milliseconds(static_cast<long>(config.expected_latency_ms));
    
    // Copy custom attributes
    metadata.custom_attributes = config.custom_attributes;
    
    return metadata;
}

std::shared_ptr<LlmInteraction> ModelRegistry::createLlamaCppModel(const ModelConfig& config) {
    auto metadata = createMetadata(config);
    return std::make_shared<LlamaCppInteraction>(config.path, metadata);
}

std::shared_ptr<LlmInteraction> ModelRegistry::createOllamaModel(const ModelConfig& config) {
    auto metadata = createMetadata(config);
    return std::make_shared<OllamaInteraction>(config.server_url, config.model_name, metadata);
}

void ModelRegistry::startHealthCheckThread() {
    if (m_health_check_thread && m_health_check_thread->joinable()) {
        return; // Already running
    }
    
    m_stop_health_checks = false;
    m_health_check_thread = std::make_unique<std::thread>(&ModelRegistry::healthCheckLoop, this);
    Logger::getInstance().info("ModelRegistry", "Started health check thread");
}

void ModelRegistry::stopHealthCheckThread() {
    m_stop_health_checks = true;
    if (m_health_check_thread && m_health_check_thread->joinable()) {
        m_health_check_thread->join();
        Logger::getInstance().info("ModelRegistry", "Stopped health check thread");
    }
}

void ModelRegistry::healthCheckLoop() {
    while (!m_stop_health_checks) {
        // Wait for the configured interval
        std::unique_lock<std::mutex> lock(m_registry_mutex);
        std::condition_variable cv;
        cv.wait_for(lock, m_config.health_check_interval, [this] {
            return m_stop_health_checks.load();
        });
        lock.unlock();
        
        if (m_stop_health_checks) {
            break;
        }
        
        // Perform health checks
        performHealthChecks();
    }
}

void ModelRegistry::updateStatus() {
    m_status.successfully_loaded = m_model_pool->size();
    m_status.currently_healthy = m_model_pool->getPoolStats().healthy_models;
    m_status.last_update = std::chrono::system_clock::now();
}

size_t ModelRegistry::parseMemoryString(const std::string& memory_str) const {
    if (memory_str.empty()) {
        return 0;
    }
    
    std::regex memory_regex(R"((\d+(?:\.\d+)?)\s*([KMGT]?)B?)");
    std::smatch match;
    
    if (std::regex_match(memory_str, match, memory_regex)) {
        double value = std::stod(match[1]);
        std::string unit = match[2];
        
        size_t multiplier = 1;
        if (unit == "K") multiplier = 1024;
        else if (unit == "M") multiplier = 1024 * 1024;
        else if (unit == "G") multiplier = 1024 * 1024 * 1024;
        else if (unit == "T") multiplier = 1024ULL * 1024 * 1024 * 1024;
        
        return static_cast<size_t>(value * multiplier);
    }
    
    return 0;
}

} // namespace Camus