// =================================================================
// tests/ModelAbstractionTest.cpp
// =================================================================
// Unit tests for Model Abstraction Layer components.

#include "Camus/ModelCapabilities.hpp"
#include "Camus/ModelPool.hpp"
#include "Camus/LlmInteraction.hpp"
#include <iostream>
#include <cassert>
#include <memory>
#include <vector>

// Mock LLM implementation for testing
class MockLlmInteraction : public Camus::LlmInteraction {
public:
    MockLlmInteraction(const std::string& model_id, const Camus::ModelMetadata& metadata)
        : m_model_id(model_id), m_metadata(metadata), m_is_healthy(true) {}

    std::string getCompletion(const std::string& prompt) override {
        return "Mock response to: " + prompt;
    }

    Camus::InferenceResponse getCompletionWithMetadata(const Camus::InferenceRequest& request) override {
        Camus::InferenceResponse response;
        response.text = getCompletion(request.prompt);
        response.response_time = std::chrono::milliseconds(100);
        response.tokens_generated = 50;
        response.finish_reason = "stop";
        return response;
    }

    Camus::ModelMetadata getModelMetadata() const override {
        return m_metadata;
    }

    bool isHealthy() const override {
        return m_is_healthy;
    }

    bool performHealthCheck() override {
        // Always healthy for mock
        m_is_healthy = true;
        return true;
    }

    Camus::ModelPerformance getCurrentPerformance() const override {
        Camus::ModelPerformance perf;
        perf.tokens_per_second = 50.0;
        perf.avg_latency = std::chrono::milliseconds(100);
        perf.memory_usage_gb = 2.0;
        perf.max_context_tokens = m_metadata.performance.max_context_tokens;
        perf.max_output_tokens = m_metadata.performance.max_output_tokens;
        return perf;
    }

    std::string getModelId() const override {
        return m_model_id;
    }

    void setHealthy(bool healthy) {
        m_is_healthy = healthy;
        m_metadata.is_healthy = healthy;
        m_metadata.is_available = healthy;
    }

private:
    std::string m_model_id;
    mutable Camus::ModelMetadata m_metadata;
    mutable bool m_is_healthy;
};

class ModelAbstractionTest {
public:
    void testModelCapabilityUtils() {
        std::cout << "Testing ModelCapability utilities..." << std::endl;

        // Test capability to string conversion
        assert(Camus::ModelCapabilityUtils::capabilityToString(Camus::ModelCapability::FAST_INFERENCE) == "FAST_INFERENCE");
        assert(Camus::ModelCapabilityUtils::capabilityToString(Camus::ModelCapability::CODE_SPECIALIZED) == "CODE_SPECIALIZED");

        // Test string to capability conversion
        assert(Camus::ModelCapabilityUtils::stringToCapability("HIGH_QUALITY") == Camus::ModelCapability::HIGH_QUALITY);
        assert(Camus::ModelCapabilityUtils::stringToCapability("SECURITY_FOCUSED") == Camus::ModelCapability::SECURITY_FOCUSED);

        // Test invalid string handling
        bool exception_thrown = false;
        try {
            Camus::ModelCapabilityUtils::stringToCapability("INVALID_CAPABILITY");
        } catch (const std::invalid_argument&) {
            exception_thrown = true;
        }
        assert(exception_thrown && "Should throw exception for invalid capability");

        // Test getAllCapabilities
        auto all_caps = Camus::ModelCapabilityUtils::getAllCapabilities();
        assert(!all_caps.empty() && "Should return non-empty capability list");
        assert(all_caps.size() == 10 && "Should return all 10 capabilities");

        // Test capability string conversion helpers
        std::vector<Camus::ModelCapability> test_caps = {
            Camus::ModelCapability::FAST_INFERENCE,
            Camus::ModelCapability::CODE_SPECIALIZED
        };
        auto cap_strings = Camus::ModelCapabilityUtils::capabilitiesToStrings(test_caps);
        assert(cap_strings.size() == 2 && "Should convert all capabilities");
        assert(cap_strings[0] == "FAST_INFERENCE" && "First capability should match");

        auto parsed_caps = Camus::ModelCapabilityUtils::parseCapabilities(cap_strings);
        assert(parsed_caps.size() == 2 && "Should parse all capabilities");
        assert(parsed_caps[0] == Camus::ModelCapability::FAST_INFERENCE && "Parsed capability should match");

        std::cout << "✓ ModelCapability utilities test passed" << std::endl;
    }

    void testModelMetadata() {
        std::cout << "Testing ModelMetadata structure..." << std::endl;

        Camus::ModelMetadata metadata;
        metadata.name = "TestModel";
        metadata.description = "A test model";
        metadata.version = "1.0";
        metadata.provider = "test_provider";
        metadata.capabilities = {Camus::ModelCapability::FAST_INFERENCE, Camus::ModelCapability::CODE_SPECIALIZED};
        metadata.performance.max_context_tokens = 4096;
        metadata.performance.max_output_tokens = 2048;
        metadata.is_available = true;
        metadata.is_healthy = true;

        // Test hasCapability function
        assert(Camus::ModelCapabilityUtils::hasCapability(metadata, Camus::ModelCapability::FAST_INFERENCE));
        assert(Camus::ModelCapabilityUtils::hasCapability(metadata, Camus::ModelCapability::CODE_SPECIALIZED));
        assert(!Camus::ModelCapabilityUtils::hasCapability(metadata, Camus::ModelCapability::HIGH_QUALITY));

        std::cout << "✓ ModelMetadata test passed" << std::endl;
    }

    void testModelPool() {
        std::cout << "Testing ModelPool functionality..." << std::endl;

        Camus::ConcreteModelPool pool;

        // Test empty pool
        assert(pool.isEmpty() && "Pool should be empty initially");
        assert(pool.size() == 0 && "Pool size should be 0");

        // Create test models
        Camus::ModelMetadata fast_metadata;
        fast_metadata.name = "FastModel";
        fast_metadata.capabilities = {Camus::ModelCapability::FAST_INFERENCE, Camus::ModelCapability::CODE_SPECIALIZED};
        fast_metadata.performance.max_context_tokens = 4096;
        fast_metadata.is_healthy = true;
        fast_metadata.is_available = true;

        Camus::ModelMetadata quality_metadata;
        quality_metadata.name = "QualityModel";
        quality_metadata.capabilities = {Camus::ModelCapability::HIGH_QUALITY, Camus::ModelCapability::REASONING};
        quality_metadata.performance.max_context_tokens = 8192;
        quality_metadata.is_healthy = true;
        quality_metadata.is_available = true;

        auto fast_model = std::make_shared<MockLlmInteraction>("fast_model", fast_metadata);
        auto quality_model = std::make_shared<MockLlmInteraction>("quality_model", quality_metadata);

        // Test adding models
        assert(pool.addModel(fast_model) && "Should successfully add fast model");
        assert(pool.addModel(quality_model) && "Should successfully add quality model");
        assert(!pool.isEmpty() && "Pool should not be empty after adding models");
        assert(pool.size() == 2 && "Pool should have 2 models");

        // Test duplicate model addition
        assert(!pool.addModel(fast_model) && "Should not add duplicate model");

        // Test getting models
        auto retrieved_model = pool.getModel("fast_model");
        assert(retrieved_model != nullptr && "Should retrieve fast model");
        assert(retrieved_model->getModelId() == "fast_model" && "Retrieved model should have correct ID");

        auto non_existent = pool.getModel("non_existent");
        assert(non_existent == nullptr && "Should return nullptr for non-existent model");

        // Test getAllModelIds
        auto model_ids = pool.getAllModelIds();
        assert(model_ids.size() == 2 && "Should return 2 model IDs");

        // Test model selection
        Camus::ModelSelectionCriteria criteria;
        criteria.required_capabilities = {Camus::ModelCapability::FAST_INFERENCE};
        
        auto selected = pool.selectModel(criteria);
        assert(selected != nullptr && "Should select a model matching criteria");
        assert(selected->getModelId() == "fast_model" && "Should select the fast model");

        // Test selection with no matches
        criteria.required_capabilities = {Camus::ModelCapability::CREATIVE_WRITING};
        selected = pool.selectModel(criteria);
        assert(selected == nullptr && "Should return nullptr when no models match");

        // Test health checks
        size_t healthy_count = pool.performHealthChecks();
        assert(healthy_count == 2 && "Both models should be healthy");

        // Test model removal
        assert(pool.removeModel("fast_model") && "Should remove fast model");
        assert(pool.size() == 1 && "Pool should have 1 model after removal");
        assert(!pool.removeModel("fast_model") && "Should not remove non-existent model");

        std::cout << "✓ ModelPool test passed" << std::endl;
    }

    void testModelSelectionCriteria() {
        std::cout << "Testing model selection criteria..." << std::endl;

        Camus::ConcreteModelPool pool;

        // Create models with different characteristics
        Camus::ModelMetadata fast_metadata;
        fast_metadata.name = "FastModel";
        fast_metadata.capabilities = {Camus::ModelCapability::FAST_INFERENCE, Camus::ModelCapability::CODE_SPECIALIZED};
        fast_metadata.performance.max_context_tokens = 4096;
        fast_metadata.performance.avg_latency = std::chrono::milliseconds(100);
        fast_metadata.is_healthy = true;
        fast_metadata.is_available = true;

        Camus::ModelMetadata quality_metadata;
        quality_metadata.name = "QualityModel";
        quality_metadata.capabilities = {Camus::ModelCapability::HIGH_QUALITY, Camus::ModelCapability::LARGE_CONTEXT};
        quality_metadata.performance.max_context_tokens = 16384;
        quality_metadata.performance.avg_latency = std::chrono::milliseconds(500);
        quality_metadata.is_healthy = true;
        quality_metadata.is_available = true;

        Camus::ModelMetadata security_metadata;
        security_metadata.name = "SecurityModel";
        security_metadata.capabilities = {Camus::ModelCapability::SECURITY_FOCUSED, Camus::ModelCapability::CODE_SPECIALIZED};
        security_metadata.performance.max_context_tokens = 8192;
        security_metadata.performance.avg_latency = std::chrono::milliseconds(300);
        security_metadata.is_healthy = true;
        security_metadata.is_available = true;

        auto fast_model = std::make_shared<MockLlmInteraction>("fast_model", fast_metadata);
        auto quality_model = std::make_shared<MockLlmInteraction>("quality_model", quality_metadata);
        auto security_model = std::make_shared<MockLlmInteraction>("security_model", security_metadata);

        pool.addModel(fast_model);
        pool.addModel(quality_model);
        pool.addModel(security_model);

        // Test context token filtering
        Camus::ModelSelectionCriteria criteria;
        criteria.min_context_tokens = 8192;
        
        auto matching = pool.getModelsMatching(criteria);
        assert(matching.size() == 2 && "Should find 2 models with 8192+ context tokens");

        // Test latency filtering
        criteria = Camus::ModelSelectionCriteria();
        criteria.max_latency_ms = 200.0;
        
        matching = pool.getModelsMatching(criteria);
        assert(matching.size() == 1 && "Should find 1 model with <200ms latency");

        // Test capability requirements
        criteria = Camus::ModelSelectionCriteria();
        criteria.required_capabilities = {Camus::ModelCapability::CODE_SPECIALIZED};
        
        matching = pool.getModelsMatching(criteria);
        assert(matching.size() == 2 && "Should find 2 models with CODE_SPECIALIZED capability");

        // Test multiple requirements
        criteria.required_capabilities = {Camus::ModelCapability::CODE_SPECIALIZED, Camus::ModelCapability::SECURITY_FOCUSED};
        
        matching = pool.getModelsMatching(criteria);
        assert(matching.size() == 1 && "Should find 1 model with both capabilities");

        std::cout << "✓ Model selection criteria test passed" << std::endl;
    }

    void testPoolStatistics() {
        std::cout << "Testing pool statistics..." << std::endl;

        Camus::ConcreteModelPool pool;
        
        // Test initial stats
        auto stats = pool.getPoolStats();
        assert(stats.total_models == 0 && "Initial total should be 0");
        assert(stats.available_models == 0 && "Initial available should be 0");
        assert(stats.healthy_models == 0 && "Initial healthy should be 0");

        // Add models
        Camus::ModelMetadata metadata;
        metadata.name = "TestModel";
        metadata.is_healthy = true;
        metadata.is_available = true;

        auto model1 = std::make_shared<MockLlmInteraction>("model1", metadata);
        auto model2 = std::make_shared<MockLlmInteraction>("model2", metadata);
        
        pool.addModel(model1);
        pool.addModel(model2);

        stats = pool.getPoolStats();
        assert(stats.total_models == 2 && "Should have 2 total models");
        assert(stats.available_models == 2 && "Should have 2 available models");
        assert(stats.healthy_models == 2 && "Should have 2 healthy models");

        // Make one model unhealthy
        std::static_pointer_cast<MockLlmInteraction>(model1)->setHealthy(false);
        
        // Force health check update
        pool.performHealthChecks();
        stats = pool.getPoolStats();
        assert(stats.healthy_models == 1 && "Should have 1 healthy model after health check");

        std::cout << "✓ Pool statistics test passed" << std::endl;
    }

    void testWarmUpAndCleanup() {
        std::cout << "Testing warmup and cleanup..." << std::endl;

        Camus::ConcreteModelPool pool;
        
        Camus::ModelMetadata metadata;
        metadata.name = "TestModel";
        metadata.is_healthy = true;
        metadata.is_available = true;

        auto model1 = std::make_shared<MockLlmInteraction>("model1", metadata);
        auto model2 = std::make_shared<MockLlmInteraction>("model2", metadata);
        
        pool.addModel(model1);
        pool.addModel(model2);

        // Test warmup
        size_t warmed_up = pool.warmUpAll();
        assert(warmed_up == 2 && "Should warm up both models");

        // Test cleanup
        pool.cleanupAll();
        assert(pool.isEmpty() && "Pool should be empty after cleanup");

        std::cout << "✓ Warmup and cleanup test passed" << std::endl;
    }

    void runAllTests() {
        std::cout << "Running Model Abstraction Layer tests..." << std::endl;
        std::cout << "===============================================" << std::endl << std::endl;

        testModelCapabilityUtils();
        std::cout << std::endl;

        testModelMetadata();
        std::cout << std::endl;

        testModelPool();
        std::cout << std::endl;

        testModelSelectionCriteria();
        std::cout << std::endl;

        testPoolStatistics();
        std::cout << std::endl;

        testWarmUpAndCleanup();
        std::cout << std::endl;

        std::cout << "All Model Abstraction Layer tests passed!" << std::endl;
    }
};

int main() {
    try {
        ModelAbstractionTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All Model Abstraction Layer component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}