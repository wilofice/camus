// =================================================================
// tests/ModelRegistryTest.cpp
// =================================================================
// Unit tests for ModelRegistry component.

#include "Camus/ModelRegistry.hpp"
#include "Camus/ModelCapabilities.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

class ModelRegistryTest {
private:
    std::string test_config_path = "test_models.yml";
    
public:
    ModelRegistryTest() {
        // Create test configuration file
        createTestConfig();
    }
    
    ~ModelRegistryTest() {
        // Clean up test files
        if (fs::exists(test_config_path)) {
            fs::remove(test_config_path);
        }
    }
    
    void createTestConfig() {
        std::ofstream config(test_config_path);
        config << R"(
models:
  test_model_1:
    type: "test_type"
    path: "/test/path/model1.gguf"
    name: "Test Model 1"
    description: "First test model"
    version: "1.0"
    capabilities:
      - "FAST_INFERENCE"
      - "CODE_SPECIALIZED"
    performance:
      max_context_tokens: 4096
      max_output_tokens: 2048
      memory_usage_gb: 2.0
      expected_tokens_per_second: 50.0
      expected_latency_ms: 200

  test_model_2:
    type: "test_type"
    path: "/test/path/model2.gguf"
    name: "Test Model 2"
    description: "Second test model"
    version: "2.0"
    capabilities:
      - "HIGH_QUALITY"
      - "LARGE_CONTEXT"
    performance:
      max_context_tokens: 16384
      max_output_tokens: 4096
      memory_usage_gb: 8.0
      expected_tokens_per_second: 20.0
      expected_latency_ms: 500

  invalid_model:
    # Missing required 'type' field
    path: "/test/path/invalid.gguf"
    name: "Invalid Model"
)";
        config.close();
    }
    
    void testConfigParsing() {
        std::cout << "Testing configuration file parsing..." << std::endl;
        
        Camus::RegistryConfig config;
        config.config_file_path = test_config_path;
        config.auto_discover = false;  // Don't auto-load
        config.enable_health_checks = false;
        
        Camus::ModelRegistry registry(config);
        
        auto configs = registry.getConfiguredModels();
        assert(configs.empty() && "Should have no models before loading");
        
        // Load configuration
        auto status = registry.loadFromConfig(test_config_path);
        
        assert(status.total_configured >= 2 && "Should parse at least 2 valid models");
        assert(status.failed_to_load >= 2 && "Should fail to load test models (no factory)");
        
        std::cout << "✓ Configuration parsing test passed" << std::endl;
    }
    
    void testModelValidation() {
        std::cout << "Testing model configuration validation..." << std::endl;
        
        Camus::RegistryConfig config;
        config.auto_discover = false;
        config.enable_health_checks = false;
        config.validate_on_load = true;
        
        Camus::ModelRegistry registry(config);
        
        // Test valid configuration
        Camus::ModelConfig valid_config;
        valid_config.name = "valid_model";
        valid_config.type = "test_type";
        valid_config.path = "/test/path.gguf";
        valid_config.capabilities = {"FAST_INFERENCE", "CODE_SPECIALIZED"};
        
        assert(registry.validateModelConfig(valid_config) && "Valid config should pass validation");
        
        // Test missing name
        Camus::ModelConfig no_name = valid_config;
        no_name.name = "";
        assert(!registry.validateModelConfig(no_name) && "Config without name should fail");
        
        // Test missing type
        Camus::ModelConfig no_type = valid_config;
        no_type.type = "";
        assert(!registry.validateModelConfig(no_type) && "Config without type should fail");
        
        // Test invalid capability
        Camus::ModelConfig bad_cap = valid_config;
        bad_cap.capabilities = {"INVALID_CAPABILITY"};
        assert(!registry.validateModelConfig(bad_cap) && "Config with invalid capability should fail");
        
        std::cout << "✓ Model validation test passed" << std::endl;
    }
    
    void testModelFactoryRegistration() {
        std::cout << "Testing model factory registration..." << std::endl;
        
        Camus::RegistryConfig config;
        config.auto_discover = false;
        config.enable_health_checks = false;
        
        Camus::ModelRegistry registry(config);
        
        bool factory_called = false;
        
        // Register a test factory
        registry.registerModelFactory("test_factory", 
            [&factory_called](const Camus::ModelConfig& cfg) {
                factory_called = true;
                return nullptr; // Return null for this test
            });
        
        // Try to load a model with the test factory
        Camus::ModelConfig test_config;
        test_config.name = "test_model";
        test_config.type = "test_factory";
        test_config.path = "/test/path";
        
        auto result = registry.loadModel(test_config);
        
        assert(factory_called && "Factory should have been called");
        assert(!result.success && "Load should fail with null model");
        
        std::cout << "✓ Model factory registration test passed" << std::endl;
    }
    
    void testRegistryStatus() {
        std::cout << "Testing registry status tracking..." << std::endl;
        
        Camus::RegistryConfig config;
        config.config_file_path = test_config_path;
        config.auto_discover = false;
        config.enable_health_checks = false;
        
        Camus::ModelRegistry registry(config);
        
        // Get initial status
        auto status = registry.getStatus();
        assert(status.total_configured == 0 && "Should have no models configured initially");
        assert(status.successfully_loaded == 0 && "Should have no models loaded initially");
        
        // Load configuration
        status = registry.loadFromConfig(test_config_path);
        assert(status.total_configured >= 2 && "Should have configured models after loading");
        assert(status.load_results.size() >= 2 && "Should have load results");
        
        std::cout << "✓ Registry status test passed" << std::endl;
    }
    
    void testHealthChecks() {
        std::cout << "Testing health check functionality..." << std::endl;
        
        Camus::RegistryConfig config;
        config.auto_discover = false;
        config.enable_health_checks = false;
        
        Camus::ModelRegistry registry(config);
        
        // Perform health checks on empty registry
        size_t healthy_count = registry.performHealthChecks();
        assert(healthy_count == 0 && "Should have no healthy models in empty registry");
        
        std::cout << "✓ Health check test passed" << std::endl;
    }
    
    void testModelInfoFormatting() {
        std::cout << "Testing model info formatting..." << std::endl;
        
        Camus::RegistryConfig config;
        config.auto_discover = false;
        config.enable_health_checks = false;
        
        Camus::ModelRegistry registry(config);
        
        // Test with no models
        std::string all_info = registry.getAllModelsInfo();
        assert(!all_info.empty() && "Should return status info even with no models");
        assert(all_info.find("No models loaded") != std::string::npos && 
               "Should indicate no models loaded");
        
        // Test with invalid model name
        std::string model_info = registry.getModelInfo("non_existent");
        assert(model_info.find("not found") != std::string::npos && 
               "Should indicate model not found");
        
        std::cout << "✓ Model info formatting test passed" << std::endl;
    }
    
    void testConfigReload() {
        std::cout << "Testing configuration reload..." << std::endl;
        
        Camus::RegistryConfig config;
        config.config_file_path = test_config_path;
        config.auto_discover = false;
        config.enable_health_checks = false;
        
        Camus::ModelRegistry registry(config);
        
        // Load initial configuration
        auto status1 = registry.loadFromConfig(test_config_path);
        size_t initial_count = status1.total_configured;
        
        // Reload configuration
        auto status2 = registry.reloadConfiguration();
        assert(status2.total_configured == initial_count && 
               "Reload should have same number of models");
        
        std::cout << "✓ Configuration reload test passed" << std::endl;
    }
    
    void testMemoryStringParsing() {
        std::cout << "Testing memory string parsing..." << std::endl;
        
        // This functionality is private, but we can test it indirectly
        // through model configuration parsing
        
        Camus::RegistryConfig config;
        config.config_file_path = test_config_path;
        config.auto_discover = false;
        config.enable_health_checks = false;
        
        Camus::ModelRegistry registry(config);
        registry.loadFromConfig(test_config_path);
        
        auto configs = registry.getConfiguredModels();
        bool found_2gb = false;
        bool found_8gb = false;
        
        for (const auto& cfg : configs) {
            if (cfg.memory_usage == "2GB" || cfg.memory_usage == "2.0GB") {
                found_2gb = true;
            }
            if (cfg.memory_usage == "8GB" || cfg.memory_usage == "8.0GB") {
                found_8gb = true;
            }
        }
        
        // We can't directly test the parsing, but we can verify the configs were loaded
        assert((found_2gb || found_8gb) && "Should find at least one memory configuration");
        
        std::cout << "✓ Memory string parsing test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running ModelRegistry unit tests..." << std::endl;
        std::cout << "====================================" << std::endl << std::endl;
        
        testConfigParsing();
        std::cout << std::endl;
        
        testModelValidation();
        std::cout << std::endl;
        
        testModelFactoryRegistration();
        std::cout << std::endl;
        
        testRegistryStatus();
        std::cout << std::endl;
        
        testHealthChecks();
        std::cout << std::endl;
        
        testModelInfoFormatting();
        std::cout << std::endl;
        
        testConfigReload();
        std::cout << std::endl;
        
        testMemoryStringParsing();
        std::cout << std::endl;
        
        std::cout << "All ModelRegistry tests passed!" << std::endl;
    }
};

int main() {
    try {
        ModelRegistryTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All ModelRegistry component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}