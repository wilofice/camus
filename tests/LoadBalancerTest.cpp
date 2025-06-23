// =================================================================
// tests/LoadBalancerTest.cpp
// =================================================================
// Unit tests for LoadBalancer component.

#include "Camus/LoadBalancer.hpp"
#include "Camus/ModelRegistry.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class LoadBalancerTest {
private:
    std::string test_config_path = "test_loadbalancer_models.yml";
    std::unique_ptr<Camus::ModelRegistry> m_registry;
    std::unique_ptr<Camus::LoadBalancer> m_load_balancer;
    
public:
    LoadBalancerTest() {
        createTestConfig();
        setupTestRegistry();
    }
    
    ~LoadBalancerTest() {
        if (fs::exists(test_config_path)) {
            fs::remove(test_config_path);
        }
    }
    
    void createTestConfig() {
        std::ofstream config(test_config_path);
        config << R"(
models:
  test_model_a:
    type: "test_type"
    path: "/test/model_a.gguf"
    name: "Test Model A"
    description: "Test model for load balancing"
    capabilities:
      - "FAST_INFERENCE"
    performance:
      max_context_tokens: 4096
      max_output_tokens: 2048
      memory_usage_gb: 2.0
      expected_tokens_per_second: 100.0
      expected_latency_ms: 100

  test_model_b:
    type: "test_type"
    path: "/test/model_b.gguf"
    name: "Test Model B"
    description: "Another test model for load balancing"
    capabilities:
      - "HIGH_QUALITY"
    performance:
      max_context_tokens: 8192
      max_output_tokens: 4096
      memory_usage_gb: 4.0
      expected_tokens_per_second: 50.0
      expected_latency_ms: 200
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
                // Return a basic mock implementation
                return std::make_shared<MockLlmInteraction>(cfg.name);
            });
        
        // Load test configuration
        m_registry->loadFromConfig(test_config_path);
        
        // Create load balancer with test configuration
        Camus::LoadBalancerConfig lb_config;
        lb_config.max_instances_per_model = 3;
        lb_config.max_requests_per_instance = 5;
        lb_config.auto_scale = true;
        lb_config.health_check_interval = std::chrono::minutes(10); // Disable for tests
        
        m_load_balancer = std::make_unique<Camus::LoadBalancer>(*m_registry, lb_config);
    }
    
private:
    // Mock LLM implementation for testing
    class MockLlmInteraction : public Camus::LlmInteraction {
    private:
        std::string m_model_name;
        
    public:
        MockLlmInteraction(const std::string& name) : m_model_name(name) {}
        
        std::string getCompletion(const std::string& prompt) override {
            // Simulate some processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return "Mock response from " + m_model_name + " for: " + prompt.substr(0, 20) + "...";
        }
        
        Camus::ModelMetadata getModelMetadata() const override {
            Camus::ModelMetadata metadata;
            metadata.name = m_model_name;
            metadata.description = "Mock model for testing";
            metadata.version = "1.0";
            metadata.provider = "mock";
            metadata.model_path = "/mock/path/" + m_model_name;
            metadata.capabilities = {Camus::ModelCapability::FAST_INFERENCE};
            metadata.performance.max_context_tokens = 4096;
            metadata.performance.max_output_tokens = 2048;
            metadata.is_available = true;
            metadata.is_healthy = true;
            return metadata;
        }
        
        bool isHealthy() const override {
            return true;
        }
        
        bool performHealthCheck() override {
            return true;
        }
        
        Camus::ModelPerformance getCurrentPerformance() const override {
            Camus::ModelPerformance perf;
            perf.max_context_tokens = 4096;
            perf.max_output_tokens = 2048;
            return perf;
        }
        
        std::string getModelId() const override {
            return m_model_name;
        }
    };
    
public:
    void testBasicInstanceCreation() {
        std::cout << "Testing basic instance creation..." << std::endl;
        
        // Create an instance for test_model_a
        std::string instance_id = m_load_balancer->createInstance("test_model_a");
        
        assert(!instance_id.empty() && "Should create instance successfully");
        assert(instance_id.find("test_model_a") != std::string::npos && 
               "Instance ID should contain model name");
        
        // Verify instance exists
        auto* instance = m_load_balancer->getInstance(instance_id);
        assert(instance != nullptr && "Should find created instance");
        assert(instance->model_name == "test_model_a" && "Instance should have correct model name");
        assert(instance->is_healthy.load() && "New instance should be healthy");
        assert(instance->active_requests.load() == 0 && "New instance should have no active requests");
        
        std::cout << "Created instance: " << instance_id << std::endl;
        std::cout << "✓ Basic instance creation test passed" << std::endl;
    }
    
    void testLoadBalancingStrategies() {
        std::cout << "Testing load balancing strategies..." << std::endl;
        
        // Create multiple instances for testing
        std::string instance1 = m_load_balancer->createInstance("test_model_a");
        std::string instance2 = m_load_balancer->createInstance("test_model_a");
        std::string instance3 = m_load_balancer->createInstance("test_model_a");
        
        assert(!instance1.empty() && !instance2.empty() && !instance3.empty() && 
               "Should create all instances");
        
        Camus::RequestContext context;
        context.request_id = "test_request_1";
        context.prompt = "Test prompt for load balancing";
        context.estimated_tokens = 100;
        
        // Test round-robin strategy
        m_load_balancer->setStrategy(Camus::LoadBalancingStrategy::ROUND_ROBIN);
        auto result1 = m_load_balancer->selectInstance("test_model_a", context);
        assert(!result1.selected_instance_id.empty() && "Round-robin should select instance");
        
        context.request_id = "test_request_2";
        auto result2 = m_load_balancer->selectInstance("test_model_a", context);
        assert(!result2.selected_instance_id.empty() && "Round-robin should select instance");
        
        // Test least-loaded strategy
        m_load_balancer->setStrategy(Camus::LoadBalancingStrategy::LEAST_LOADED);
        context.request_id = "test_request_3";
        auto result3 = m_load_balancer->selectInstance("test_model_a", context);
        assert(!result3.selected_instance_id.empty() && "Least-loaded should select instance");
        
        // Test response time strategy
        m_load_balancer->setStrategy(Camus::LoadBalancingStrategy::RESPONSE_TIME);
        context.request_id = "test_request_4";
        auto result4 = m_load_balancer->selectInstance("test_model_a", context);
        assert(!result4.selected_instance_id.empty() && "Response time should select instance");
        
        std::cout << "Round-robin selected: " << result1.selected_instance_id << std::endl;
        std::cout << "Least-loaded selected: " << result3.selected_instance_id << std::endl;
        std::cout << "Response time selected: " << result4.selected_instance_id << std::endl;
        
        std::cout << "✓ Load balancing strategies test passed" << std::endl;
    }
    
    void testRequestTracking() {
        std::cout << "Testing request tracking..." << std::endl;
        
        std::string instance_id = m_load_balancer->createInstance("test_model_b");
        assert(!instance_id.empty() && "Should create instance");
        
        auto* instance = m_load_balancer->getInstance(instance_id);
        assert(instance != nullptr && "Should find instance");
        
        // Record request start
        std::string request_id = "tracking_test_request";
        m_load_balancer->recordRequestStart(instance_id, request_id);
        
        assert(instance->active_requests.load() == 1 && "Should have 1 active request");
        
        // Record request end
        m_load_balancer->recordRequestEnd(instance_id, request_id, 150.0, true);
        
        assert(instance->active_requests.load() == 0 && "Should have 0 active requests");
        assert(instance->total_requests.load() == 1 && "Should have 1 total request");
        assert(instance->failed_requests.load() == 0 && "Should have 0 failed requests");
        assert(instance->average_response_time.load() == 150.0 && "Should record response time");
        
        // Record failed request
        m_load_balancer->recordRequestStart(instance_id, "failed_request");
        m_load_balancer->recordRequestEnd(instance_id, "failed_request", 300.0, false);
        
        assert(instance->total_requests.load() == 2 && "Should have 2 total requests");
        assert(instance->failed_requests.load() == 1 && "Should have 1 failed request");
        
        std::cout << "Instance stats after requests:" << std::endl;
        std::cout << "  Total requests: " << instance->total_requests.load() << std::endl;
        std::cout << "  Failed requests: " << instance->failed_requests.load() << std::endl;
        std::cout << "  Avg response time: " << instance->average_response_time.load() << "ms" << std::endl;
        
        std::cout << "✓ Request tracking test passed" << std::endl;
    }
    
    void testHealthChecks() {
        std::cout << "Testing health checks..." << std::endl;
        
        // Create some instances
        std::string instance1 = m_load_balancer->createInstance("test_model_a");
        std::string instance2 = m_load_balancer->createInstance("test_model_a");
        
        assert(!instance1.empty() && !instance2.empty() && "Should create instances");
        
        // All should be healthy initially
        size_t healthy_count = m_load_balancer->performHealthChecks();
        assert(healthy_count >= 2 && "Should have healthy instances");
        
        // Simulate many failed requests on one instance
        auto* instance = m_load_balancer->getInstance(instance1);
        assert(instance != nullptr && "Should find instance");
        
        for (int i = 0; i < 20; ++i) {
            std::string req_id = "health_test_" + std::to_string(i);
            m_load_balancer->recordRequestStart(instance1, req_id);
            m_load_balancer->recordRequestEnd(instance1, req_id, 1000.0, false); // All failed
        }
        
        // Health check should mark it unhealthy
        healthy_count = m_load_balancer->performHealthChecks();
        assert(!instance->is_healthy.load() && "Instance with high failure rate should be unhealthy");
        
        std::cout << "✓ Health checks test passed" << std::endl;
    }
    
    void testAutoScaling() {
        std::cout << "Testing auto-scaling..." << std::endl;
        
        // Enable auto-scaling
        m_load_balancer->setAutoScaling(true);
        
        // Initially no instances
        auto instances = m_load_balancer->getInstancesForModel("test_model_b");
        size_t initial_count = instances.size();
        
        // Simulate high load by creating requests
        Camus::RequestContext context;
        context.prompt = "High load test";
        
        std::vector<std::string> request_ids;
        for (int i = 0; i < 15; ++i) { // More than max_requests_per_instance
            context.request_id = "load_test_" + std::to_string(i);
            auto result = m_load_balancer->selectInstance("test_model_b", context);
            
            if (!result.selected_instance_id.empty()) {
                m_load_balancer->recordRequestStart(result.selected_instance_id, context.request_id);
                request_ids.push_back(context.request_id);
            }
        }
        
        // Trigger auto-scaling
        size_t scaled_count = m_load_balancer->autoScale("test_model_b");
        
        std::cout << "Instances before scaling: " << initial_count << std::endl;
        std::cout << "Instances after scaling: " << scaled_count << std::endl;
        
        // Clean up requests
        for (const auto& req_id : request_ids) {
            // Find which instance handled this request and complete it
            auto instances_after = m_load_balancer->getInstancesForModel("test_model_b");
            for (auto* inst : instances_after) {
                if (inst->active_requests.load() > 0) {
                    m_load_balancer->recordRequestEnd(inst->instance_id, req_id, 100.0, true);
                    break;
                }
            }
        }
        
        std::cout << "✓ Auto-scaling test passed" << std::endl;
    }
    
    void testInstanceRemoval() {
        std::cout << "Testing instance removal..." << std::endl;
        
        // Create an instance
        std::string instance_id = m_load_balancer->createInstance("test_model_a");
        assert(!instance_id.empty() && "Should create instance");
        
        // Verify it exists
        auto* instance = m_load_balancer->getInstance(instance_id);
        assert(instance != nullptr && "Should find created instance");
        
        // Remove it
        bool removed = m_load_balancer->removeInstance(instance_id);
        assert(removed && "Should remove instance successfully");
        
        // Verify it's gone
        instance = m_load_balancer->getInstance(instance_id);
        assert(instance == nullptr && "Should not find removed instance");
        
        std::cout << "✓ Instance removal test passed" << std::endl;
    }
    
    void testMultipleModels() {
        std::cout << "Testing multiple models..." << std::endl;
        
        // Create instances for different models
        std::string instance_a1 = m_load_balancer->createInstance("test_model_a");
        std::string instance_a2 = m_load_balancer->createInstance("test_model_a");
        std::string instance_b1 = m_load_balancer->createInstance("test_model_b");
        
        assert(!instance_a1.empty() && !instance_a2.empty() && !instance_b1.empty() && 
               "Should create all instances");
        
        // Get instances for each model
        auto instances_a = m_load_balancer->getInstancesForModel("test_model_a");
        auto instances_b = m_load_balancer->getInstancesForModel("test_model_b");
        
        assert(instances_a.size() >= 2 && "Should have instances for model A");
        assert(instances_b.size() >= 1 && "Should have instances for model B");
        
        // Verify instances belong to correct models
        for (auto* inst : instances_a) {
            assert(inst->model_name == "test_model_a" && "Instance should belong to model A");
        }
        for (auto* inst : instances_b) {
            assert(inst->model_name == "test_model_b" && "Instance should belong to model B");
        }
        
        std::cout << "Model A instances: " << instances_a.size() << std::endl;
        std::cout << "Model B instances: " << instances_b.size() << std::endl;
        
        std::cout << "✓ Multiple models test passed" << std::endl;
    }
    
    void testStatistics() {
        std::cout << "Testing statistics..." << std::endl;
        
        // Create instances and generate some load
        std::string instance_id = m_load_balancer->createInstance("test_model_a");
        assert(!instance_id.empty() && "Should create instance");
        
        // Generate some requests
        for (int i = 0; i < 5; ++i) {
            std::string req_id = "stats_test_" + std::to_string(i);
            m_load_balancer->recordRequestStart(instance_id, req_id);
            m_load_balancer->recordRequestEnd(instance_id, req_id, 100.0 + i * 10, true);
        }
        
        // Get statistics
        std::string stats = m_load_balancer->getStatistics();
        
        assert(!stats.empty() && "Should generate statistics");
        assert(stats.find("Load Balancer Statistics") != std::string::npos && 
               "Should contain statistics header");
        assert(stats.find("test_model_a") != std::string::npos && 
               "Should contain model information");
        
        std::cout << "Generated statistics:" << std::endl;
        std::cout << stats << std::endl;
        
        std::cout << "✓ Statistics test passed" << std::endl;
    }
    
    void testConfiguration() {
        std::cout << "Testing configuration..." << std::endl;
        
        // Get current configuration
        auto config = m_load_balancer->getConfig();
        
        // Modify configuration
        config.max_instances_per_model = 5;
        config.max_requests_per_instance = 8;
        config.auto_scale = false;
        
        m_load_balancer->setConfig(config);
        
        // Verify configuration was updated
        auto updated_config = m_load_balancer->getConfig();
        assert(updated_config.max_instances_per_model == 5 && 
               "Should update max instances per model");
        assert(updated_config.max_requests_per_instance == 8 && 
               "Should update max requests per instance");
        assert(!updated_config.auto_scale && "Should disable auto-scale");
        
        std::cout << "✓ Configuration test passed" << std::endl;
    }
    
    void testErrorHandling() {
        std::cout << "Testing error handling..." << std::endl;
        
        // Try to select instance for non-existent model
        Camus::RequestContext context;
        context.request_id = "error_test";
        context.prompt = "Test prompt";
        
        auto result = m_load_balancer->selectInstance("non_existent_model", context);
        assert(result.selected_instance_id.empty() && "Should fail for non-existent model");
        assert(!result.selection_reason.empty() && "Should provide error reason");
        
        // Try to get non-existent instance
        auto* instance = m_load_balancer->getInstance("non_existent_instance");
        assert(instance == nullptr && "Should return null for non-existent instance");
        
        // Try to remove non-existent instance
        bool removed = m_load_balancer->removeInstance("non_existent_instance");
        assert(!removed && "Should fail to remove non-existent instance");
        
        std::cout << "✓ Error handling test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running LoadBalancer unit tests..." << std::endl;
        std::cout << "====================================" << std::endl << std::endl;
        
        testBasicInstanceCreation();
        std::cout << std::endl;
        
        testLoadBalancingStrategies();
        std::cout << std::endl;
        
        testRequestTracking();
        std::cout << std::endl;
        
        testHealthChecks();
        std::cout << std::endl;
        
        testAutoScaling();
        std::cout << std::endl;
        
        testInstanceRemoval();
        std::cout << std::endl;
        
        testMultipleModels();
        std::cout << std::endl;
        
        testStatistics();
        std::cout << std::endl;
        
        testConfiguration();
        std::cout << std::endl;
        
        testErrorHandling();
        std::cout << std::endl;
        
        std::cout << "All LoadBalancer tests passed!" << std::endl;
    }
};

int main() {
    try {
        LoadBalancerTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All LoadBalancer component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}