// =================================================================
// tests/SingleModelStrategyTest.cpp
// =================================================================
// Unit tests for SingleModelStrategy component.

#include "Camus/SingleModelStrategy.hpp"
#include "Camus/ModelRegistry.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class SingleModelStrategyTest {
private:
    std::string test_config_path = "test_strategy_models.yml";
    std::unique_ptr<Camus::ModelRegistry> m_registry;
    std::unique_ptr<Camus::SingleModelStrategy> m_strategy;
    
public:
    SingleModelStrategyTest() {
        createTestConfig();
        setupTestRegistry();
    }
    
    ~SingleModelStrategyTest() {
        if (fs::exists(test_config_path)) {
            fs::remove(test_config_path);
        }
    }
    
    void createTestConfig() {
        std::ofstream config(test_config_path);
        config << R"(
models:
  code_model:
    type: "test_type"
    path: "/test/code_model.gguf"
    name: "Code Specialized Model"
    description: "Model specialized for code generation"
    capabilities:
      - "CODE_SPECIALIZED"
      - "FAST_INFERENCE"
    performance:
      max_context_tokens: 8192
      max_output_tokens: 4096
      memory_usage_gb: 4.0
      expected_tokens_per_second: 75.0
      expected_latency_ms: 150

  analysis_model:
    type: "test_type"
    path: "/test/analysis_model.gguf"
    name: "Analysis Model"
    description: "Model optimized for code analysis"
    capabilities:
      - "HIGH_QUALITY"
      - "REASONING"
      - "SECURITY_FOCUSED"
    performance:
      max_context_tokens: 16384
      max_output_tokens: 2048
      memory_usage_gb: 6.0
      expected_tokens_per_second: 50.0
      expected_latency_ms: 200

  fast_model:
    type: "test_type"
    path: "/test/fast_model.gguf"
    name: "Fast Model"
    description: "Fast model for simple queries"
    capabilities:
      - "FAST_INFERENCE"
      - "CHAT_OPTIMIZED"
    performance:
      max_context_tokens: 4096
      max_output_tokens: 1024
      memory_usage_gb: 2.0
      expected_tokens_per_second: 120.0
      expected_latency_ms: 80
)";
        config.close();
    }
    
    void setupTestRegistry() {
        Camus::RegistryConfig registry_config;
        registry_config.auto_discover = false;
        registry_config.enable_health_checks = false;
        
        m_registry = std::make_unique<Camus::ModelRegistry>(registry_config);
        
        // Register test factory that creates mock models
        m_registry->registerModelFactory("test_type", 
            [](const Camus::ModelConfig& cfg) -> std::shared_ptr<Camus::LlmInteraction> {
                // Convert capability strings to ModelCapability enums
                std::vector<Camus::ModelCapability> capabilities;
                for (const auto& cap_str : cfg.capabilities) {
                    if (cap_str == "CODE_SPECIALIZED") capabilities.push_back(Camus::ModelCapability::CODE_SPECIALIZED);
                    else if (cap_str == "FAST_INFERENCE") capabilities.push_back(Camus::ModelCapability::FAST_INFERENCE);
                    else if (cap_str == "HIGH_QUALITY") capabilities.push_back(Camus::ModelCapability::HIGH_QUALITY);
                    else if (cap_str == "REASONING") capabilities.push_back(Camus::ModelCapability::REASONING);
                    else if (cap_str == "SECURITY_FOCUSED") capabilities.push_back(Camus::ModelCapability::SECURITY_FOCUSED);
                    else if (cap_str == "CHAT_OPTIMIZED") capabilities.push_back(Camus::ModelCapability::CHAT_OPTIMIZED);
                }
                return std::make_shared<MockLlmInteraction>(cfg.name, capabilities);
            });
        
        // Load test configuration
        m_registry->loadFromConfig(test_config_path);
        
        // Create strategy with test configuration
        Camus::SingleModelStrategyConfig strategy_config;
        strategy_config.enable_prompt_optimization = true;
        strategy_config.enable_context_management = true;
        strategy_config.enable_parameter_tuning = true;
        strategy_config.enable_response_validation = true;
        strategy_config.min_quality_threshold = 0.3;
        
        m_strategy = std::make_unique<Camus::SingleModelStrategy>(*m_registry, strategy_config);
    }
    
private:
    // Mock LLM implementation for testing
    class MockLlmInteraction : public Camus::LlmInteraction {
    private:
        std::string m_model_name;
        std::vector<Camus::ModelCapability> m_capabilities;
        
    public:
        MockLlmInteraction(const std::string& name, const std::vector<Camus::ModelCapability>& capabilities) 
            : m_model_name(name), m_capabilities(capabilities) {}
        
        std::string getCompletion(const std::string& prompt) override {
            // Simulate processing time based on model type
            if (m_model_name.find("fast") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            } else if (m_model_name.find("analysis") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
            
            // Generate responses based on prompt content and model capabilities
            std::string response = "Response from " + m_model_name + ":\n\n";
            
            if (prompt.find("code") != std::string::npos || prompt.find("function") != std::string::npos) {
                if (hasCapability(Camus::ModelCapability::CODE_SPECIALIZED)) {
                    response += "```cpp\n";
                    response += "// Generated code for your request\n";
                    response += "int example_function() {\n";
                    response += "    // Implementation here\n";
                    response += "    return 0;\n";
                    response += "}\n";
                    response += "```\n\n";
                    response += "This code implements the requested functionality with proper error handling.";
                } else {
                    response += "Here's a code solution for your request:\n\n";
                    response += "int example() { return 0; }";
                }
            } else if (prompt.find("analyze") != std::string::npos || prompt.find("analysis") != std::string::npos) {
                if (hasCapability(Camus::ModelCapability::REASONING)) {
                    response += "Detailed Analysis:\n\n";
                    response += "1. Structure: The code follows standard patterns\n";
                    response += "2. Complexity: O(n) time complexity\n";
                    response += "3. Security: No obvious vulnerabilities detected\n";
                    response += "4. Recommendations: Consider adding input validation\n";
                } else {
                    response += "Basic analysis: The code appears to be functional.";
                }
            } else if (prompt.find("refactor") != std::string::npos) {
                response += "Refactored code with improvements:\n\n";
                response += "// Improved version with better naming and structure\n";
                response += "void improved_function() {\n    // Refactored implementation\n}";
            } else {
                if (hasCapability(Camus::ModelCapability::FAST_INFERENCE)) {
                    response += "Quick answer: " + prompt.substr(0, 50) + "...";
                } else {
                    response += "Detailed response to your query about: " + prompt.substr(0, 100) + "...";
                }
            }
            
            return response;
        }
        
        Camus::ModelMetadata getModelMetadata() const override {
            Camus::ModelMetadata metadata;
            metadata.name = m_model_name;
            metadata.description = "Mock model for testing";
            metadata.version = "1.0";
            metadata.provider = "mock";
            metadata.model_path = "/mock/path/" + m_model_name;
            metadata.capabilities = m_capabilities;
            
            if (m_model_name.find("fast") != std::string::npos) {
                metadata.performance.max_context_tokens = 4096;
                metadata.performance.max_output_tokens = 1024;
                metadata.performance.avg_latency = std::chrono::milliseconds(80);
            } else if (m_model_name.find("analysis") != std::string::npos) {
                metadata.performance.max_context_tokens = 16384;
                metadata.performance.max_output_tokens = 2048;
                metadata.performance.avg_latency = std::chrono::milliseconds(200);
            } else {
                metadata.performance.max_context_tokens = 8192;
                metadata.performance.max_output_tokens = 4096;
                metadata.performance.avg_latency = std::chrono::milliseconds(150);
            }
            
            metadata.is_available = true;
            metadata.is_healthy = true;
            return metadata;
        }
        
        bool isHealthy() const override { return true; }
        bool performHealthCheck() override { return true; }
        
        Camus::ModelPerformance getCurrentPerformance() const override {
            return getModelMetadata().performance;
        }
        
        std::string getModelId() const override { return m_model_name; }
        
    private:
        bool hasCapability(Camus::ModelCapability capability) const {
            return std::find(m_capabilities.begin(), m_capabilities.end(), capability) != m_capabilities.end();
        }
    };
    
public:
    void testBasicExecution() {
        std::cout << "Testing basic strategy execution..." << std::endl;
        
        Camus::StrategyRequest request;
        request.request_id = "test_basic_001";
        request.prompt = "Write a simple hello world function";
        request.selected_model = "code_model";
        request.task_type = Camus::TaskType::CODE_GENERATION;
        request.max_tokens = 512;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Basic execution should succeed");
        assert(response.request_id == request.request_id && "Response should have correct request ID");
        assert(!response.response_text.empty() && "Response should have text");
        assert(response.model_used == request.selected_model && "Response should indicate correct model");
        assert(response.total_time.count() > 0 && "Response should record processing time");
        assert(response.quality_score > 0.0 && "Response should have quality score");
        
        std::cout << "Model used: " << response.model_used << std::endl;
        std::cout << "Quality score: " << response.quality_score << std::endl;
        std::cout << "Total time: " << response.total_time.count() << "ms" << std::endl;
        std::cout << "Optimization steps: " << response.optimization_steps.size() << std::endl;
        
        std::cout << "✓ Basic execution test passed" << std::endl;
    }
    
    void testPromptOptimization() {
        std::cout << "Testing prompt optimization..." << std::endl;
        
        Camus::StrategyRequest request;
        request.request_id = "test_optimization_001";
        request.prompt = "make function";
        request.selected_model = "code_model";
        request.task_type = Camus::TaskType::CODE_GENERATION;
        request.enable_optimization = true;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Optimization test should succeed");
        assert(!response.optimized_prompt.empty() && "Should have optimized prompt");
        assert(response.optimized_prompt != request.prompt && "Prompt should be optimized");
        assert(response.optimization_time.count() >= 0 && "Should record optimization time");
        assert(std::find(response.optimization_steps.begin(), response.optimization_steps.end(), 
                        "prompt_optimization") != response.optimization_steps.end() && 
               "Should include prompt optimization step");
        
        std::cout << "Original prompt length: " << request.prompt.length() << std::endl;
        std::cout << "Optimized prompt length: " << response.optimized_prompt.length() << std::endl;
        std::cout << "Optimization time: " << response.optimization_time.count() << "ms" << std::endl;
        
        std::cout << "✓ Prompt optimization test passed" << std::endl;
    }
    
    void testContextManagement() {
        std::cout << "Testing context window management..." << std::endl;
        
        // Create a request with long context
        Camus::StrategyRequest request;
        request.request_id = "test_context_001";
        request.prompt = "Analyze this code";
        request.context = std::string(10000, 'x'); // Very long context
        request.selected_model = "analysis_model";
        request.task_type = Camus::TaskType::CODE_ANALYSIS;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Context management test should succeed");
        assert(response.context_utilization >= 0.0 && response.context_utilization <= 1.0 && 
               "Context utilization should be valid ratio");
        assert(std::find(response.optimization_steps.begin(), response.optimization_steps.end(), 
                        "context_management") != response.optimization_steps.end() && 
               "Should include context management step");
        
        std::cout << "Context utilization: " << response.context_utilization << std::endl;
        std::cout << "Context tokens used: " << response.context_tokens_used << std::endl;
        
        std::cout << "✓ Context management test passed" << std::endl;
    }
    
    void testParameterTuning() {
        std::cout << "Testing parameter tuning..." << std::endl;
        
        Camus::StrategyRequest request;
        request.request_id = "test_tuning_001";
        request.prompt = "Generate a function to calculate fibonacci";
        request.selected_model = "code_model";
        request.task_type = Camus::TaskType::CODE_GENERATION;
        request.temperature = 0.8;
        request.max_tokens = 1024;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Parameter tuning test should succeed");
        assert(std::find(response.optimization_steps.begin(), response.optimization_steps.end(), 
                        "parameter_tuning") != response.optimization_steps.end() && 
               "Should include parameter tuning step");
        
        std::cout << "Parameter tuning applied successfully" << std::endl;
        
        std::cout << "✓ Parameter tuning test passed" << std::endl;
    }
    
    void testModelSpecificOptimizations() {
        std::cout << "Testing model-specific optimizations..." << std::endl;
        
        // Test code model optimization
        Camus::StrategyRequest code_request;
        code_request.request_id = "test_code_opt_001";
        code_request.prompt = "create a sorting function";
        code_request.selected_model = "code_model";
        code_request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto code_response = m_strategy->execute(code_request);
        assert(code_response.success && "Code model optimization should succeed");
        assert(code_response.response_text.find("```") != std::string::npos && 
               "Code model should generate formatted code");
        
        // Test analysis model optimization
        Camus::StrategyRequest analysis_request;
        analysis_request.request_id = "test_analysis_opt_001";
        analysis_request.prompt = "analyze this function for security issues";
        analysis_request.selected_model = "analysis_model";
        analysis_request.task_type = Camus::TaskType::SECURITY_REVIEW;
        
        auto analysis_response = m_strategy->execute(analysis_request);
        assert(analysis_response.success && "Analysis model optimization should succeed");
        assert(analysis_response.response_text.find("Analysis") != std::string::npos && 
               "Analysis model should provide structured analysis");
        
        // Test fast model optimization
        Camus::StrategyRequest fast_request;
        fast_request.request_id = "test_fast_opt_001";
        fast_request.prompt = "what is recursion?";
        fast_request.selected_model = "fast_model";
        fast_request.task_type = Camus::TaskType::SIMPLE_QUERY;
        
        auto fast_response = m_strategy->execute(fast_request);
        assert(fast_response.success && "Fast model optimization should succeed");
        assert(fast_response.execution_time < code_response.execution_time && 
               "Fast model should be quicker than code model");
        
        std::cout << "Code model execution time: " << code_response.execution_time.count() << "ms" << std::endl;
        std::cout << "Analysis model execution time: " << analysis_response.execution_time.count() << "ms" << std::endl;
        std::cout << "Fast model execution time: " << fast_response.execution_time.count() << "ms" << std::endl;
        
        std::cout << "✓ Model-specific optimizations test passed" << std::endl;
    }
    
    void testQualityValidation() {
        std::cout << "Testing response quality validation..." << std::endl;
        
        Camus::StrategyRequest request;
        request.request_id = "test_quality_001";
        request.prompt = "Explain object-oriented programming concepts";
        request.selected_model = "analysis_model";
        request.task_type = Camus::TaskType::CODE_ANALYSIS;
        request.enable_validation = true;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Quality validation test should succeed");
        assert(response.quality_score >= 0.0 && response.quality_score <= 1.0 && 
               "Quality score should be valid ratio");
        assert(response.validation_time.count() >= 0 && "Should record validation time");
        assert(std::find(response.optimization_steps.begin(), response.optimization_steps.end(), 
                        "response_validation") != response.optimization_steps.end() && 
               "Should include validation step");
        
        std::cout << "Quality score: " << response.quality_score << std::endl;
        std::cout << "Validation time: " << response.validation_time.count() << "ms" << std::endl;
        
        std::cout << "✓ Quality validation test passed" << std::endl;
    }
    
    void testStatisticsTracking() {
        std::cout << "Testing statistics tracking..." << std::endl;
        
        // Reset statistics first
        m_strategy->resetStatistics();
        
        // Execute several requests
        for (int i = 0; i < 5; ++i) {
            Camus::StrategyRequest request;
            request.request_id = "test_stats_" + std::to_string(i);
            request.prompt = "Generate test code " + std::to_string(i);
            request.selected_model = "code_model";
            request.task_type = Camus::TaskType::CODE_GENERATION;
            
            m_strategy->execute(request);
        }
        
        auto stats = m_strategy->getStatistics();
        
        assert(stats.total_requests >= 5 && "Should track all requests");
        assert(stats.successful_requests > 0 && "Should have successful requests");
        assert(stats.average_execution_time > 0.0 && "Should track execution time");
        assert(stats.average_quality_score > 0.0 && "Should track quality scores");
        assert(!stats.model_usage.empty() && "Should track model usage");
        assert(!stats.task_distribution.empty() && "Should track task distribution");
        
        std::cout << "Total requests: " << stats.total_requests << std::endl;
        std::cout << "Success rate: " << (stats.successful_requests * 100.0 / stats.total_requests) << "%" << std::endl;
        std::cout << "Average execution time: " << stats.average_execution_time << "ms" << std::endl;
        std::cout << "Average quality score: " << stats.average_quality_score << std::endl;
        
        std::cout << "✓ Statistics tracking test passed" << std::endl;
    }
    
    void testCustomQualityScorer() {
        std::cout << "Testing custom quality scorer..." << std::endl;
        
        // Register custom scorer
        m_strategy->registerQualityScorer(
            [](const std::string& prompt, const std::string& response) -> double {
                // Custom scorer: higher score for longer responses
                if (response.length() > 200) return 0.9;
                if (response.length() > 100) return 0.7;
                return 0.5;
            });
        
        Camus::StrategyRequest request;
        request.request_id = "test_custom_scorer_001";
        request.prompt = "Write a detailed explanation of algorithms";
        request.selected_model = "analysis_model";
        request.task_type = Camus::TaskType::CODE_ANALYSIS;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Custom scorer test should succeed");
        assert(response.quality_score > 0.0 && "Should have quality score from custom scorer");
        
        std::cout << "Custom quality score: " << response.quality_score << std::endl;
        std::cout << "Response length: " << response.response_text.length() << std::endl;
        
        std::cout << "✓ Custom quality scorer test passed" << std::endl;
    }
    
    void testParameterAdaptation() {
        std::cout << "Testing parameter adaptation..." << std::endl;
        
        // Enable adaptive parameters
        auto config = m_strategy->getConfig();
        config.parameter_config.enable_adaptive_parameters = true;
        config.parameter_config.adaptation_learning_rate = 0.2;
        m_strategy->setConfig(config);
        
        Camus::StrategyRequest request;
        request.request_id = "test_adaptation_001";
        request.prompt = "Create a test function";
        request.selected_model = "code_model";
        request.task_type = Camus::TaskType::CODE_GENERATION;
        
        // Execute multiple requests to trigger adaptation
        for (int i = 0; i < 3; ++i) {
            request.request_id = "test_adaptation_" + std::to_string(i);
            auto response = m_strategy->execute(request);
            assert(response.success && "Adaptation test requests should succeed");
        }
        
        std::cout << "Parameter adaptation tested with multiple requests" << std::endl;
        
        std::cout << "✓ Parameter adaptation test passed" << std::endl;
    }
    
    void testConfigurationManagement() {
        std::cout << "Testing configuration management..." << std::endl;
        
        // Get current configuration
        auto original_config = m_strategy->getConfig();
        
        // Modify configuration
        Camus::SingleModelStrategyConfig new_config = original_config;
        new_config.enable_prompt_optimization = false;
        new_config.min_quality_threshold = 0.8;
        new_config.context_config.utilization_target = 0.9;
        
        m_strategy->setConfig(new_config);
        
        // Verify configuration was updated
        auto updated_config = m_strategy->getConfig();
        assert(updated_config.enable_prompt_optimization == false && 
               "Should update optimization setting");
        assert(updated_config.min_quality_threshold == 0.8 && 
               "Should update quality threshold");
        assert(updated_config.context_config.utilization_target == 0.9 && 
               "Should update context utilization target");
        
        // Test with disabled optimization
        Camus::StrategyRequest request;
        request.request_id = "test_config_001";
        request.prompt = "test prompt";
        request.selected_model = "code_model";
        request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto response = m_strategy->execute(request);
        assert(response.success && "Should work with modified config");
        assert(std::find(response.optimization_steps.begin(), response.optimization_steps.end(), 
                        "prompt_optimization") == response.optimization_steps.end() && 
               "Should not include optimization when disabled");
        
        std::cout << "✓ Configuration management test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running SingleModelStrategy unit tests..." << std::endl;
        std::cout << "===============================================" << std::endl << std::endl;
        
        testBasicExecution();
        std::cout << std::endl;
        
        testPromptOptimization();
        std::cout << std::endl;
        
        testContextManagement();
        std::cout << std::endl;
        
        testParameterTuning();
        std::cout << std::endl;
        
        testModelSpecificOptimizations();
        std::cout << std::endl;
        
        testQualityValidation();
        std::cout << std::endl;
        
        testStatisticsTracking();
        std::cout << std::endl;
        
        testCustomQualityScorer();
        std::cout << std::endl;
        
        testParameterAdaptation();
        std::cout << std::endl;
        
        testConfigurationManagement();
        std::cout << std::endl;
        
        std::cout << "All SingleModelStrategy tests passed!" << std::endl;
    }
};

int main() {
    try {
        SingleModelStrategyTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All SingleModelStrategy component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}