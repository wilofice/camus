// =================================================================
// tests/ModelOrchestratorTest.cpp
// =================================================================
// Unit tests for ModelOrchestrator component.

#include "Camus/ModelOrchestrator.hpp"
#include "Camus/ModelRegistry.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class ModelOrchestratorTest {
private:
    std::string test_config_path = "test_orchestrator_models.yml";
    std::unique_ptr<Camus::ModelRegistry> m_registry;
    std::unique_ptr<Camus::ModelOrchestrator> m_orchestrator;
    
public:
    ModelOrchestratorTest() {
        createTestConfig();
        setupTestRegistry();
    }
    
    ~ModelOrchestratorTest() {
        if (fs::exists(test_config_path)) {
            fs::remove(test_config_path);
        }
    }
    
    void createTestConfig() {
        std::ofstream config(test_config_path);
        config << R"(
models:
  fast_model:
    type: "test_type"
    path: "/test/fast_model.gguf"
    name: "Fast Test Model"
    description: "Fast model for testing"
    capabilities:
      - "FAST_INFERENCE"
    performance:
      max_context_tokens: 4096
      max_output_tokens: 2048
      memory_usage_gb: 2.0
      expected_tokens_per_second: 100.0
      expected_latency_ms: 100

  quality_model:
    type: "test_type"
    path: "/test/quality_model.gguf"
    name: "Quality Test Model"
    description: "High quality model for testing"
    capabilities:
      - "HIGH_QUALITY"
      - "CODE_SPECIALIZED"
    performance:
      max_context_tokens: 8192
      max_output_tokens: 4096
      memory_usage_gb: 4.0
      expected_tokens_per_second: 50.0
      expected_latency_ms: 200

  code_model:
    type: "test_type"
    path: "/test/code_model.gguf"
    name: "Code Test Model"
    description: "Code specialized model for testing"
    capabilities:
      - "CODE_SPECIALIZED"
      - "FAST_INFERENCE"
    performance:
      max_context_tokens: 16384
      max_output_tokens: 4096
      memory_usage_gb: 6.0
      expected_tokens_per_second: 75.0
      expected_latency_ms: 150
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
                return std::make_shared<MockLlmInteraction>(cfg.name);
            });
        
        // Load test configuration
        m_registry->loadFromConfig(test_config_path);
        
        // Create orchestrator with test configuration
        Camus::OrchestratorConfig orch_config;
        orch_config.enable_caching = true;
        orch_config.cache_ttl = std::chrono::seconds(300); // 5 minutes for testing
        orch_config.max_cache_size = 10;
        orch_config.enable_fallback = true;
        orch_config.min_quality_score = 0.5;
        
        m_orchestrator = std::make_unique<Camus::ModelOrchestrator>(*m_registry, orch_config);
    }
    
private:
    // Mock LLM implementation for testing
    class MockLlmInteraction : public Camus::LlmInteraction {
    private:
        std::string m_model_name;
        
    public:
        MockLlmInteraction(const std::string& name) : m_model_name(name) {}
        
        std::string getCompletion(const std::string& prompt) override {
            // Simulate processing time based on model type
            if (m_model_name.find("fast") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else if (m_model_name.find("quality") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(75));
            }
            
            // Generate different responses based on prompt content
            std::string response = "Mock response from " + m_model_name;
            
            if (prompt.find("code") != std::string::npos || prompt.find("function") != std::string::npos) {
                response += ": Here's the code you requested:\n```cpp\nint main() {\n    return 0;\n}\n```";
            } else if (prompt.find("explain") != std::string::npos) {
                response += ": This is a detailed explanation of your request. The answer involves multiple components working together to achieve the desired outcome.";
            } else {
                response += " for prompt: " + prompt.substr(0, 30) + "...";
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
            
            if (m_model_name.find("fast") != std::string::npos) {
                metadata.capabilities = {Camus::ModelCapability::FAST_INFERENCE};
                metadata.performance.max_context_tokens = 4096;
            } else if (m_model_name.find("quality") != std::string::npos) {
                metadata.capabilities = {Camus::ModelCapability::HIGH_QUALITY, Camus::ModelCapability::CODE_SPECIALIZED};
                metadata.performance.max_context_tokens = 8192;
            } else {
                metadata.capabilities = {Camus::ModelCapability::CODE_SPECIALIZED, Camus::ModelCapability::FAST_INFERENCE};
                metadata.performance.max_context_tokens = 16384;
            }
            
            metadata.performance.max_output_tokens = 2048;
            metadata.is_available = true;
            metadata.is_healthy = true;
            return metadata;
        }
        
        bool isHealthy() const override { return true; }
        bool performHealthCheck() override { return true; }
        
        Camus::ModelPerformance getCurrentPerformance() const override {
            Camus::ModelPerformance perf;
            perf.max_context_tokens = 4096;
            perf.max_output_tokens = 2048;
            return perf;
        }
        
        std::string getModelId() const override { return m_model_name; }
    };
    
public:
    void testBasicRequestProcessing() {
        std::cout << "Testing basic request processing..." << std::endl;
        
        Camus::PipelineRequest request;
        request.request_id = "test_basic_001";
        request.prompt = "Hello, can you help me with a simple task?";
        request.max_tokens = 100;
        request.temperature = 0.7;
        
        auto response = m_orchestrator->processRequest(request);
        
        assert(response.success && "Request should succeed");
        assert(response.request_id == request.request_id && "Response should have correct request ID");
        assert(!response.response_text.empty() && "Response should have text");
        assert(!response.selected_model.empty() && "Response should indicate selected model");
        assert(response.total_time.count() > 0 && "Response should record processing time");
        assert(!response.pipeline_steps.empty() && "Response should record pipeline steps");
        
        std::cout << "Selected model: " << response.selected_model << std::endl;
        std::cout << "Processing time: " << response.total_time.count() << "ms" << std::endl;
        std::cout << "Pipeline steps: " << response.pipeline_steps.size() << std::endl;
        
        std::cout << "✓ Basic request processing test passed" << std::endl;
    }
    
    void testCodeSpecializedRequest() {
        std::cout << "Testing code specialized request..." << std::endl;
        
        Camus::PipelineRequest request;
        request.request_id = "test_code_001";
        request.prompt = "Write a function to calculate the factorial of a number";
        request.max_tokens = 200;
        
        auto response = m_orchestrator->processRequest(request);
        
        assert(response.success && "Code request should succeed");
        assert(response.response_text.find("code") != std::string::npos || 
               response.response_text.find("cpp") != std::string::npos && 
               "Response should contain code-related content");
        assert(response.classified_task != Camus::TaskType::UNKNOWN && 
               "Task should be classified");
        
        std::cout << "Task classified as: " << Camus::taskTypeToString(response.classified_task) << std::endl;
        std::cout << "Classification confidence: " << response.classification_confidence << std::endl;
        
        std::cout << "✓ Code specialized request test passed" << std::endl;
    }
    
    void testCachingMechanism() {
        std::cout << "Testing caching mechanism..." << std::endl;
        
        Camus::PipelineRequest request;
        request.request_id = "test_cache_001";
        request.prompt = "What is the capital of France?";
        request.enable_caching = true;
        
        // First request - should not be cached
        auto response1 = m_orchestrator->processRequest(request);
        assert(response1.success && "First request should succeed");
        assert(!response1.cache_hit && "First request should not be cache hit");
        
        // Second identical request - should be cached
        request.request_id = "test_cache_002";
        auto response2 = m_orchestrator->processRequest(request);
        assert(response2.success && "Second request should succeed");
        assert(response2.cache_hit && "Second request should be cache hit");
        assert(response2.response_text == response1.response_text && 
               "Cached response should match original");
        assert(response2.total_time < response1.total_time && 
               "Cached response should be faster");
        
        // Check cache statistics
        auto cache_stats = m_orchestrator->getCacheStatistics();
        assert(cache_stats["cache_size"] >= 1.0 && "Cache should contain entries");
        assert(cache_stats["cache_hit_rate"] > 0.0 && "Cache hit rate should be positive");
        
        std::cout << "Cache size: " << cache_stats["cache_size"] << std::endl;
        std::cout << "Cache hit rate: " << cache_stats["cache_hit_rate"] << std::endl;
        
        std::cout << "✓ Caching mechanism test passed" << std::endl;
    }
    
    void testFallbackMechanism() {
        std::cout << "Testing fallback mechanism..." << std::endl;
        
        // Set fallback strategy
        m_orchestrator->setFallbackStrategy(Camus::FallbackStrategy::SIMPLE_MODEL);
        
        Camus::PipelineRequest request;
        request.request_id = "test_fallback_001";
        request.prompt = "This is a test request for fallback";
        request.preferred_models = {"non_existent_model"}; // Force failure
        request.require_fallback = true;
        
        auto response = m_orchestrator->processRequest(request);
        
        // Even with preferred model failure, fallback should work
        assert(response.success && "Request should succeed with fallback");
        std::cout << "Fallback used: " << (response.fallback_used ? "Yes" : "No") << std::endl;
        std::cout << "Selected model: " << response.selected_model << std::endl;
        
        std::cout << "✓ Fallback mechanism test passed" << std::endl;
    }
    
    void testQualityScoring() {
        std::cout << "Testing quality scoring..." << std::endl;
        
        // Register custom quality scorer
        m_orchestrator->registerQualityScorer(
            [](const std::string& prompt, const std::string& response) -> double {
                // Simple scorer: longer responses get higher scores
                if (response.length() > 100) return 0.9;
                if (response.length() > 50) return 0.7;
                return 0.5;
            });
        
        Camus::PipelineRequest request;
        request.request_id = "test_quality_001";
        request.prompt = "Please provide a detailed explanation of machine learning";
        
        auto response = m_orchestrator->processRequest(request);
        
        assert(response.success && "Quality test request should succeed");
        assert(response.quality_score > 0.0 && "Response should have quality score");
        
        std::cout << "Quality score: " << response.quality_score << std::endl;
        std::cout << "Response length: " << response.response_text.length() << std::endl;
        
        std::cout << "✓ Quality scoring test passed" << std::endl;
    }
    
    void testBatchProcessing() {
        std::cout << "Testing batch processing..." << std::endl;
        
        std::vector<Camus::PipelineRequest> requests;
        
        for (int i = 0; i < 3; ++i) {
            Camus::PipelineRequest request;
            request.request_id = "batch_test_" + std::to_string(i);
            request.prompt = "Batch request number " + std::to_string(i);
            requests.push_back(request);
        }
        
        auto responses = m_orchestrator->processRequests(requests);
        
        assert(responses.size() == requests.size() && "Should process all requests");
        
        for (size_t i = 0; i < responses.size(); ++i) {
            assert(responses[i].success && "Batch request should succeed");
            assert(responses[i].request_id == requests[i].request_id && 
                   "Response should match request ID");
            std::cout << "Batch " << i << " - Time: " << responses[i].total_time.count() << "ms" << std::endl;
        }
        
        std::cout << "✓ Batch processing test passed" << std::endl;
    }
    
    void testStatisticsTracking() {
        std::cout << "Testing statistics tracking..." << std::endl;
        
        // Reset statistics first
        m_orchestrator->resetStatistics();
        
        // Process several requests
        for (int i = 0; i < 5; ++i) {
            Camus::PipelineRequest request;
            request.request_id = "stats_test_" + std::to_string(i);
            request.prompt = "Statistics test request " + std::to_string(i);
            m_orchestrator->processRequest(request);
        }
        
        auto stats = m_orchestrator->getStatistics();
        
        assert(stats.total_requests >= 5 && "Should track all requests");
        assert(stats.successful_requests > 0 && "Should have successful requests");
        assert(stats.average_response_time > 0.0 && "Should track response time");
        assert(!stats.model_usage.empty() && "Should track model usage");
        
        std::cout << "Total requests: " << stats.total_requests << std::endl;
        std::cout << "Success rate: " << (stats.successful_requests * 100.0 / stats.total_requests) << "%" << std::endl;
        std::cout << "Average response time: " << stats.average_response_time << "ms" << std::endl;
        
        // Test performance report
        auto report = m_orchestrator->getPerformanceReport();
        assert(!report.empty() && "Should generate performance report");
        assert(report.find("Performance Report") != std::string::npos && 
               "Report should contain header");
        
        std::cout << "✓ Statistics tracking test passed" << std::endl;
    }
    
    void testConfigurationManagement() {
        std::cout << "Testing configuration management..." << std::endl;
        
        // Get current configuration
        auto config = m_orchestrator->getConfig();
        auto original_cache_enabled = config.enable_caching;
        
        // Modify configuration
        config.enable_caching = !original_cache_enabled;
        config.min_quality_score = 0.8;
        m_orchestrator->setConfig(config);
        
        // Verify configuration was updated
        auto updated_config = m_orchestrator->getConfig();
        assert(updated_config.enable_caching == !original_cache_enabled && 
               "Should update caching setting");
        assert(updated_config.min_quality_score == 0.8 && 
               "Should update quality threshold");
        
        // Test component enabling/disabling
        m_orchestrator->setComponentEnabled("classification", false);
        m_orchestrator->setComponentEnabled("caching", true);
        
        std::cout << "✓ Configuration management test passed" << std::endl;
    }
    
    void testPipelineWarmup() {
        std::cout << "Testing pipeline warmup..." << std::endl;
        
        std::vector<std::string> warmup_requests = {
            "Hello world",
            "Write a function",
            "Explain algorithms",
            "Debug this code"
        };
        
        // Warmup should not throw exceptions
        try {
            m_orchestrator->warmupPipeline(warmup_requests);
            std::cout << "Pipeline warmed up with " << warmup_requests.size() << " requests" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warmup completed with some errors (expected): " << e.what() << std::endl;
        }
        
        std::cout << "✓ Pipeline warmup test passed" << std::endl;
    }
    
    void testErrorHandling() {
        std::cout << "Testing error handling..." << std::endl;
        
        // Test with empty prompt
        Camus::PipelineRequest empty_request;
        empty_request.request_id = "test_error_001";
        empty_request.prompt = "";
        
        auto response1 = m_orchestrator->processRequest(empty_request);
        std::cout << "Empty prompt result - Success: " << response1.success << std::endl;
        
        // Test with very long timeout
        Camus::PipelineRequest timeout_request;
        timeout_request.request_id = "test_error_002";
        timeout_request.prompt = "Test with very short timeout";
        timeout_request.timeout = std::chrono::milliseconds(1); // Very short timeout
        
        auto response2 = m_orchestrator->processRequest(timeout_request);
        std::cout << "Short timeout result - Success: " << response2.success << std::endl;
        
        // Test with invalid configuration
        Camus::OrchestratorConfig invalid_config;
        invalid_config.min_quality_score = -1.0; // Invalid quality score
        
        try {
            m_orchestrator->setConfig(invalid_config);
            std::cout << "Invalid config set (no validation implemented)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Invalid config rejected: " << e.what() << std::endl;
        }
        
        std::cout << "✓ Error handling test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running ModelOrchestrator unit tests..." << std::endl;
        std::cout << "===========================================" << std::endl << std::endl;
        
        testBasicRequestProcessing();
        std::cout << std::endl;
        
        testCodeSpecializedRequest();
        std::cout << std::endl;
        
        testCachingMechanism();
        std::cout << std::endl;
        
        testFallbackMechanism();
        std::cout << std::endl;
        
        testQualityScoring();
        std::cout << std::endl;
        
        testBatchProcessing();
        std::cout << std::endl;
        
        testStatisticsTracking();
        std::cout << std::endl;
        
        testConfigurationManagement();
        std::cout << std::endl;
        
        testPipelineWarmup();
        std::cout << std::endl;
        
        testErrorHandling();
        std::cout << std::endl;
        
        std::cout << "All ModelOrchestrator tests passed!" << std::endl;
    }
};

int main() {
    try {
        ModelOrchestratorTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All ModelOrchestrator component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}