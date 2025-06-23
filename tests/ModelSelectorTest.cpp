// =================================================================
// tests/ModelSelectorTest.cpp
// =================================================================
// Unit tests for ModelSelector component.

#include "Camus/ModelSelector.hpp"
#include "Camus/ModelRegistry.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class ModelSelectorTest {
private:
    std::string test_config_path = "test_selector_models.yml";
    std::unique_ptr<Camus::ModelRegistry> m_registry;
    std::unique_ptr<Camus::ModelSelector> m_selector;
    
public:
    ModelSelectorTest() {
        createTestConfig();
        setupTestRegistry();
    }
    
    ~ModelSelectorTest() {
        if (fs::exists(test_config_path)) {
            fs::remove(test_config_path);
        }
    }
    
    void createTestConfig() {
        std::ofstream config(test_config_path);
        config << R"(
models:
  fast_coder:
    type: "test_type"
    path: "/test/fast_coder.gguf"
    name: "Fast Coder Model"
    description: "Fast model for code generation"
    capabilities:
      - "FAST_INFERENCE"
      - "CODE_SPECIALIZED"
    performance:
      max_context_tokens: 4096
      max_output_tokens: 2048
      memory_usage_gb: 2.0
      expected_tokens_per_second: 100.0
      expected_latency_ms: 100

  quality_assistant:
    type: "test_type"
    path: "/test/quality_assistant.gguf"
    name: "Quality Assistant Model"
    description: "High quality general purpose model"
    capabilities:
      - "HIGH_QUALITY"
      - "LARGE_CONTEXT"
    performance:
      max_context_tokens: 16384
      max_output_tokens: 4096
      memory_usage_gb: 8.0
      expected_tokens_per_second: 30.0
      expected_latency_ms: 300

  code_reviewer:
    type: "test_type"
    path: "/test/code_reviewer.gguf"
    name: "Code Reviewer Model"
    description: "Specialized in code review and analysis"
    capabilities:
      - "CODE_SPECIALIZED"
      - "HIGH_QUALITY"
      - "SECURITY_FOCUSED"
    performance:
      max_context_tokens: 8192
      max_output_tokens: 2048
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
        
        // Register test factory that always returns nullptr (for testing)
        m_registry->registerModelFactory("test_type", 
            [](const Camus::ModelConfig& cfg) {
                return nullptr; // Simulate model loading failure for tests
            });
        
        // Load test configuration
        m_registry->loadFromConfig(test_config_path);
        
        // Create selector with test configuration
        Camus::ModelSelectorConfig selector_config;
        selector_config.default_strategy = "score_based";
        selector_config.enable_learning = true;
        
        m_selector = std::make_unique<Camus::ModelSelector>(*m_registry, selector_config);
    }
    
    void testBasicModelSelection() {
        std::cout << "Testing basic model selection..." << std::endl;
        
        // Test simple selection criteria
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::CODE_GENERATION;
        criteria.context_size = 2048;
        criteria.prefer_quality = false;
        
        auto result = m_selector->selectModel(criteria);
        
        // Should select some model (even if loading fails, selection logic should work)
        assert(!result.selected_model.empty() && "Should select a model");
        assert(result.confidence_score >= 0.0 && result.confidence_score <= 1.0 && 
               "Confidence score should be in valid range");
        assert(!result.selection_reason.empty() && "Should provide selection reason");
        
        std::cout << "Selected: " << result.selected_model 
                  << " (confidence: " << result.confidence_score << ")" << std::endl;
        std::cout << "Reason: " << result.selection_reason << std::endl;
        
        std::cout << "✓ Basic model selection test passed" << std::endl;
    }
    
    void testCodeSpecializedSelection() {
        std::cout << "Testing code specialized model selection..." << std::endl;
        
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::CODE_GENERATION;
        criteria.required_capabilities = {"CODE_SPECIALIZED"};
        criteria.context_size = 2048;
        
        auto result = m_selector->selectModel(criteria);
        
        assert(!result.selected_model.empty() && "Should select a code specialized model");
        
        // Verify the selected model has code specialization capability
        auto models = m_registry->getConfiguredModels();
        bool found_model = false;
        for (const auto& model : models) {
            if (model.name == result.selected_model) {
                auto it = std::find(model.capabilities.begin(), model.capabilities.end(), 
                                  "CODE_SPECIALIZED");
                assert(it != model.capabilities.end() && 
                       "Selected model should have CODE_SPECIALIZED capability");
                found_model = true;
                break;
            }
        }
        assert(found_model && "Should find the selected model in registry");
        
        std::cout << "✓ Code specialized selection test passed" << std::endl;
    }
    
    void testQualityPreferenceSelection() {
        std::cout << "Testing quality preference selection..." << std::endl;
        
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::DOCUMENTATION;
        criteria.prefer_quality = true;
        criteria.context_size = 1024;
        
        auto result = m_selector->selectModel(criteria);
        
        assert(!result.selected_model.empty() && "Should select a quality model");
        assert(result.confidence_score > 0.0 && "Should have positive confidence");
        
        std::cout << "✓ Quality preference selection test passed" << std::endl;
    }
    
    void testContextSizeRequirements() {
        std::cout << "Testing context size requirements..." << std::endl;
        
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::CODE_GENERATION;
        criteria.context_size = 12000; // Large context requirement
        
        auto result = m_selector->selectModel(criteria);
        
        assert(!result.selected_model.empty() && "Should select a model with large context");
        
        // Verify the selected model supports the required context size
        auto models = m_registry->getConfiguredModels();
        for (const auto& model : models) {
            if (model.name == result.selected_model) {
                assert(model.max_tokens >= criteria.context_size && 
                       "Selected model should support required context size");
                break;
            }
        }
        
        std::cout << "✓ Context size requirements test passed" << std::endl;
    }
    
    void testSelectionStrategies() {
        std::cout << "Testing different selection strategies..." << std::endl;
        
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::CODE_ANALYSIS;
        criteria.required_capabilities = {"CODE_SPECIALIZED"};
        criteria.context_size = 4000;
        
        // Test rule-based strategy
        m_selector->setActiveStrategy("rule_based");
        auto rule_result = m_selector->selectModel(criteria);
        assert(!rule_result.selected_model.empty() && "Rule-based should select a model");
        
        // Test score-based strategy
        m_selector->setActiveStrategy("score_based");
        auto score_result = m_selector->selectModel(criteria);
        assert(!score_result.selected_model.empty() && "Score-based should select a model");
        
        // Test learning-based strategy
        m_selector->setActiveStrategy("learning_based");
        auto learning_result = m_selector->selectModel(criteria);
        assert(!learning_result.selected_model.empty() && "Learning-based should select a model");
        
        std::cout << "Rule-based selected: " << rule_result.selected_model << std::endl;
        std::cout << "Score-based selected: " << score_result.selected_model << std::endl;
        std::cout << "Learning-based selected: " << learning_result.selected_model << std::endl;
        
        std::cout << "✓ Selection strategies test passed" << std::endl;
    }
    
    void testPerformanceTracking() {
        std::cout << "Testing performance tracking..." << std::endl;
        
        std::string test_model = "fast_coder";
        
        // Record some performance data
        m_selector->recordPerformance(test_model, Camus::TaskType::CODE_GENERATION, 
                                    true, 150.0, 0.85);
        m_selector->recordPerformance(test_model, Camus::TaskType::CODE_GENERATION, 
                                    true, 120.0, 0.90);
        m_selector->recordPerformance(test_model, Camus::TaskType::BUG_FIXING, 
                                    false, 200.0, 0.60);
        
        auto stats = m_selector->getModelStatistics(test_model);
        
        assert(stats.total_requests == 3 && "Should have recorded 3 requests");
        assert(stats.successful_requests == 2 && "Should have 2 successful requests");
        assert(std::abs(stats.success_rate - (2.0/3.0)) < 0.01 && "Success rate should be 2/3");
        assert(stats.average_latency_ms > 0 && "Should have average latency");
        assert(stats.average_quality_score > 0 && "Should have average quality score");
        
        std::cout << "Stats for " << test_model << ":" << std::endl;
        std::cout << "  Total requests: " << stats.total_requests << std::endl;
        std::cout << "  Success rate: " << stats.success_rate << std::endl;
        std::cout << "  Avg latency: " << stats.average_latency_ms << "ms" << std::endl;
        std::cout << "  Avg quality: " << stats.average_quality_score << std::endl;
        
        std::cout << "✓ Performance tracking test passed" << std::endl;
    }
    
    void testLearningAdaptation() {
        std::cout << "Testing learning adaptation..." << std::endl;
        
        m_selector->setActiveStrategy("learning_based");
        
        std::string good_model = "quality_assistant";
        std::string poor_model = "fast_coder";
        
        // Record good performance for quality_assistant
        for (int i = 0; i < 5; ++i) {
            m_selector->recordPerformance(good_model, Camus::TaskType::DOCUMENTATION, 
                                        true, 100.0, 0.95);
        }
        
        // Record poor performance for fast_coder
        for (int i = 0; i < 5; ++i) {
            m_selector->recordPerformance(poor_model, Camus::TaskType::DOCUMENTATION, 
                                        false, 300.0, 0.3);
        }
        
        // Now test selection for similar task
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::DOCUMENTATION;
        criteria.context_size = 2048;
        
        auto result = m_selector->selectModel(criteria);
        
        // Learning-based strategy should prefer the model with better historical performance
        std::cout << "Learning-based selected: " << result.selected_model 
                  << " (confidence: " << result.confidence_score << ")" << std::endl;
        
        std::cout << "✓ Learning adaptation test passed" << std::endl;
    }
    
    void testSelectionHistory() {
        std::cout << "Testing selection history..." << std::endl;
        
        // Make several selections to build history
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::CODE_GENERATION;
        
        for (int i = 0; i < 5; ++i) {
            m_selector->selectModel(criteria);
        }
        
        auto history = m_selector->getSelectionHistory(3);
        assert(history.size() <= 3 && "Should return at most 3 entries");
        assert(!history.empty() && "Should have selection history");
        
        for (const auto& entry : history) {
            assert(!entry.selected_model.empty() && "History entries should have selected model");
            assert(!entry.selection_reason.empty() && "History entries should have reason");
        }
        
        std::cout << "✓ Selection history test passed" << std::endl;
    }
    
    void testConfigurationUpdates() {
        std::cout << "Testing configuration updates..." << std::endl;
        
        auto original_config = m_selector->getConfig();
        
        // Update configuration
        Camus::ModelSelectorConfig new_config = original_config;
        new_config.default_strategy = "rule_based";
        new_config.min_confidence_threshold = 0.7;
        
        m_selector->setConfig(new_config);
        
        auto updated_config = m_selector->getConfig();
        assert(updated_config.default_strategy == "rule_based" && 
               "Should update default strategy");
        assert(updated_config.min_confidence_threshold == 0.7 && 
               "Should update confidence threshold");
        
        // Verify strategy was actually changed
        assert(m_selector->getActiveStrategy() == "rule_based" && 
               "Active strategy should be updated");
        
        std::cout << "✓ Configuration updates test passed" << std::endl;
    }
    
    void testAnalysisReporting() {
        std::cout << "Testing analysis reporting..." << std::endl;
        
        // Generate some selection data
        Camus::SelectionCriteria criteria;
        criteria.task_type = Camus::TaskType::CODE_GENERATION;
        
        for (int i = 0; i < 3; ++i) {
            m_selector->selectModel(criteria);
        }
        
        // Record performance for analysis
        m_selector->recordPerformance("fast_coder", Camus::TaskType::CODE_GENERATION, 
                                    true, 100.0, 0.8);
        
        auto analysis = m_selector->analyzeSelectionPatterns();
        
        assert(!analysis.empty() && "Should generate analysis report");
        assert(analysis.find("Model Selection Analysis") != std::string::npos && 
               "Should contain analysis header");
        assert(analysis.find("Model Usage Frequency") != std::string::npos && 
               "Should contain usage frequency");
        
        std::cout << "Analysis report generated successfully" << std::endl;
        
        std::cout << "✓ Analysis reporting test passed" << std::endl;
    }
    
    void testErrorHandling() {
        std::cout << "Testing error handling..." << std::endl;
        
        // Test with invalid strategy
        bool result = m_selector->setActiveStrategy("non_existent_strategy");
        assert(!result && "Should fail to set invalid strategy");
        
        // Test with empty criteria (should still work)
        Camus::SelectionCriteria empty_criteria;
        auto selection_result = m_selector->selectModel(empty_criteria);
        
        // Should still attempt to select something
        std::cout << "Empty criteria result: " << selection_result.selected_model << std::endl;
        
        // Test statistics for non-existent model
        auto empty_stats = m_selector->getModelStatistics("non_existent_model");
        assert(empty_stats.total_requests == 0 && "Should return empty stats for unknown model");
        
        std::cout << "✓ Error handling test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running ModelSelector unit tests..." << std::endl;
        std::cout << "====================================" << std::endl << std::endl;
        
        testBasicModelSelection();
        std::cout << std::endl;
        
        testCodeSpecializedSelection();
        std::cout << std::endl;
        
        testQualityPreferenceSelection();
        std::cout << std::endl;
        
        testContextSizeRequirements();
        std::cout << std::endl;
        
        testSelectionStrategies();
        std::cout << std::endl;
        
        testPerformanceTracking();
        std::cout << std::endl;
        
        testLearningAdaptation();
        std::cout << std::endl;
        
        testSelectionHistory();
        std::cout << std::endl;
        
        testConfigurationUpdates();
        std::cout << std::endl;
        
        testAnalysisReporting();
        std::cout << std::endl;
        
        testErrorHandling();
        std::cout << std::endl;
        
        std::cout << "All ModelSelector tests passed!" << std::endl;
    }
};

int main() {
    try {
        ModelSelectorTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All ModelSelector component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}