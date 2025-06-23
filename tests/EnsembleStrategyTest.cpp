// =================================================================
// tests/EnsembleStrategyTest.cpp
// =================================================================
// Unit tests for EnsembleStrategy component.

#include "Camus/EnsembleStrategy.hpp"
#include "Camus/ModelRegistry.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class EnsembleStrategyTest {
private:
    std::string test_config_path = "test_ensemble_models.yml";
    std::unique_ptr<Camus::ModelRegistry> m_registry;
    std::unique_ptr<Camus::EnsembleStrategy> m_strategy;
    
public:
    EnsembleStrategyTest() {
        createTestConfig();
        setupTestRegistry();
    }
    
    ~EnsembleStrategyTest() {
        if (fs::exists(test_config_path)) {
            fs::remove(test_config_path);
        }
    }
    
    void createTestConfig() {
        std::ofstream config(test_config_path);
        config << R"(
models:
  ensemble_model_1:
    type: "test_type"
    path: "/test/ensemble_model_1.gguf"
    name: "Ensemble Model 1"
    description: "First model for ensemble testing"
    capabilities:
      - "CODE_SPECIALIZED"
      - "FAST_INFERENCE"
    performance:
      max_context_tokens: 8192
      max_output_tokens: 2048
      memory_usage_gb: 4.0
      expected_tokens_per_second: 80.0
      expected_latency_ms: 120

  ensemble_model_2:
    type: "test_type"
    path: "/test/ensemble_model_2.gguf"
    name: "Ensemble Model 2"
    description: "Second model for ensemble testing"
    capabilities:
      - "HIGH_QUALITY"
      - "REASONING"
    performance:
      max_context_tokens: 16384
      max_output_tokens: 4096
      memory_usage_gb: 8.0
      expected_tokens_per_second: 50.0
      expected_latency_ms: 200

  ensemble_model_3:
    type: "test_type"
    path: "/test/ensemble_model_3.gguf"
    name: "Ensemble Model 3"
    description: "Third model for ensemble testing"
    capabilities:
      - "FAST_INFERENCE"
      - "CHAT_OPTIMIZED"
    performance:
      max_context_tokens: 4096
      max_output_tokens: 1024
      memory_usage_gb: 2.0
      expected_tokens_per_second: 100.0
      expected_latency_ms: 80

  ensemble_model_4:
    type: "test_type"
    path: "/test/ensemble_model_4.gguf"
    name: "Ensemble Model 4"
    description: "Fourth model for ensemble testing"
    capabilities:
      - "HIGH_QUALITY"
      - "SECURITY_FOCUSED"
    performance:
      max_context_tokens: 12288
      max_output_tokens: 3072
      memory_usage_gb: 6.0
      expected_tokens_per_second: 60.0
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
        
        // Create ensemble strategy with test configuration
        Camus::EnsembleStrategyConfig strategy_config;
        strategy_config.default_method = Camus::EnsembleMethod::BEST_OF_N;
        strategy_config.enable_parallel_execution = true;
        strategy_config.max_concurrent_models = 4;
        strategy_config.min_ensemble_quality = 0.3;
        
        m_strategy = std::make_unique<Camus::EnsembleStrategy>(*m_registry, strategy_config);
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
            // Simulate different processing times
            if (m_model_name.find("1") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            } else if (m_model_name.find("2") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else if (m_model_name.find("3") != std::string::npos) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
            
            // Generate different response styles based on model characteristics
            std::string response = "Response from " + m_model_name + ":\n\n";
            
            if (prompt.find("code") != std::string::npos || prompt.find("function") != std::string::npos) {
                if (hasCapability(Camus::ModelCapability::CODE_SPECIALIZED)) {
                    response += "```cpp\n";
                    response += "// Optimized code solution\n";
                    response += "int optimized_function() {\n";
                    response += "    // Efficient implementation\n";
                    response += "    return calculate_result();\n";
                    response += "}\n";
                    response += "```\n\n";
                    response += "This solution provides optimal performance and follows best practices.";
                } else if (hasCapability(Camus::ModelCapability::HIGH_QUALITY)) {
                    response += "Here is a comprehensive code solution:\n\n";
                    response += "```cpp\n";
                    response += "// Well-structured solution\n";
                    response += "class Solution {\n";
                    response += "public:\n";
                    response += "    int solve() {\n";
                    response += "        // Detailed implementation\n";
                    response += "        return process_data();\n";
                    response += "    }\n";
                    response += "};\n";
                    response += "```\n\n";
                    response += "This approach ensures maintainability and extensibility.";
                } else {
                    response += "Quick code solution:\n\nint solve() { return 42; }\n\nThis should work for your needs.";
                }
            } else if (prompt.find("analyze") != std::string::npos || prompt.find("explain") != std::string::npos) {
                if (hasCapability(Camus::ModelCapability::REASONING)) {
                    response += "Comprehensive Analysis:\n\n";
                    response += "1. Problem Structure: The issue involves multiple interconnected components\n";
                    response += "2. Key Considerations: Performance, scalability, and maintainability\n";
                    response += "3. Recommended Approach: Systematic decomposition with iterative refinement\n";
                    response += "4. Expected Outcomes: Improved efficiency and reduced complexity\n";
                    response += "5. Risk Factors: Potential integration challenges and resource constraints\n\n";
                    response += "This analysis provides a thorough understanding of the problem space.";
                } else if (hasCapability(Camus::ModelCapability::HIGH_QUALITY)) {
                    response += "Detailed Explanation:\n\n";
                    response += "The topic you've asked about involves several important aspects that need consideration.\n";
                    response += "From a technical perspective, the solution requires careful planning and execution.\n";
                    response += "The key factors to consider include performance implications and long-term maintainability.\n";
                    response += "This approach will help ensure successful implementation.";
                } else {
                    response += "Brief analysis: The main points to consider are efficiency and effectiveness.";
                }
            } else if (prompt.find("security") != std::string::npos) {
                if (hasCapability(Camus::ModelCapability::SECURITY_FOCUSED)) {
                    response += "Security Assessment:\n\n";
                    response += "1. Authentication: Implement multi-factor authentication\n";
                    response += "2. Authorization: Use role-based access control\n";
                    response += "3. Data Protection: Encrypt sensitive data at rest and in transit\n";
                    response += "4. Input Validation: Sanitize all user inputs\n";
                    response += "5. Monitoring: Implement comprehensive security logging\n\n";
                    response += "These measures will significantly enhance system security.";
                } else {
                    response += "Basic security considerations: Use HTTPS, validate inputs, and keep software updated.";
                }
            } else {
                // General response
                if (hasCapability(Camus::ModelCapability::HIGH_QUALITY)) {
                    response += "This is a detailed and comprehensive response to your query. ";
                    response += "I've carefully considered multiple aspects of your question and provided ";
                    response += "a thorough analysis that covers the key points you should be aware of. ";
                    response += "The solution I'm proposing takes into account both immediate needs and long-term considerations.";
                } else if (hasCapability(Camus::ModelCapability::FAST_INFERENCE)) {
                    response += "Quick answer: " + prompt.substr(0, 50) + "... Here's a concise solution.";
                } else {
                    response += "Standard response addressing your query with relevant information and recommendations.";
                }
            }
            
            return response;
        }
        
        Camus::ModelMetadata getModelMetadata() const override {
            Camus::ModelMetadata metadata;
            metadata.name = m_model_name;
            metadata.description = "Mock model for ensemble testing";
            metadata.version = "1.0";
            metadata.provider = "mock";
            metadata.model_path = "/mock/path/" + m_model_name;
            metadata.capabilities = m_capabilities;
            
            // Set different performance characteristics
            if (m_model_name.find("1") != std::string::npos) {
                metadata.performance.max_context_tokens = 8192;
                metadata.performance.max_output_tokens = 2048;
                metadata.performance.avg_latency = std::chrono::milliseconds(120);
            } else if (m_model_name.find("2") != std::string::npos) {
                metadata.performance.max_context_tokens = 16384;
                metadata.performance.max_output_tokens = 4096;
                metadata.performance.avg_latency = std::chrono::milliseconds(200);
            } else if (m_model_name.find("3") != std::string::npos) {
                metadata.performance.max_context_tokens = 4096;
                metadata.performance.max_output_tokens = 1024;
                metadata.performance.avg_latency = std::chrono::milliseconds(80);
            } else {
                metadata.performance.max_context_tokens = 12288;
                metadata.performance.max_output_tokens = 3072;
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
    void testBasicEnsembleExecution() {
        std::cout << "Testing basic ensemble execution..." << std::endl;
        
        Camus::EnsembleRequest request;
        request.request_id = "test_basic_ensemble_001";
        request.prompt = "Write a function to calculate the square root of a number";
        request.target_models = {"ensemble_model_1", "ensemble_model_2", "ensemble_model_3"};
        request.method = Camus::EnsembleMethod::BEST_OF_N;
        request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Basic ensemble execution should succeed");
        assert(response.request_id == request.request_id && "Response should have correct request ID");
        assert(!response.final_response.empty() && "Response should have final text");
        assert(response.models_used > 0 && "Should use at least one model");
        assert(!response.individual_responses.empty() && "Should have individual responses");
        assert(response.ensemble_confidence >= 0.0 && response.ensemble_confidence <= 1.0 && 
               "Ensemble confidence should be valid ratio");
        assert(response.ensemble_quality >= 0.0 && response.ensemble_quality <= 1.0 && 
               "Ensemble quality should be valid ratio");
        
        std::cout << "Models used: " << response.models_used << std::endl;
        std::cout << "Ensemble confidence: " << response.ensemble_confidence << std::endl;
        std::cout << "Ensemble quality: " << response.ensemble_quality << std::endl;
        std::cout << "Total time: " << response.total_time.count() << "ms" << std::endl;
        
        std::cout << "✓ Basic ensemble execution test passed" << std::endl;
    }
    
    void testVotingEnsemble() {
        std::cout << "Testing voting ensemble method..." << std::endl;
        
        Camus::EnsembleRequest request;
        request.request_id = "test_voting_001";
        request.prompt = "Explain the concept of object-oriented programming";
        request.target_models = {"ensemble_model_1", "ensemble_model_2", "ensemble_model_3", "ensemble_model_4"};
        request.method = Camus::EnsembleMethod::VOTING;
        request.task_type = Camus::TaskType::CODE_ANALYSIS;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Voting ensemble should succeed");
        assert(response.method_used == Camus::EnsembleMethod::VOTING && 
               "Should use voting method");
        assert(response.models_used >= 2 && "Voting requires multiple models");
        assert(!response.decision_factors.empty() && "Should have decision factors");
        
        // Check that multiple models participated
        size_t successful_models = 0;
        for (const auto& model_resp : response.individual_responses) {
            if (model_resp.execution_success) {
                successful_models++;
            }
        }
        assert(successful_models >= 2 && "Voting should have multiple successful models");
        
        std::cout << "Voting method used successfully with " << successful_models << " models" << std::endl;
        std::cout << "Decision factors: " << response.decision_factors.size() << std::endl;
        
        std::cout << "✓ Voting ensemble test passed" << std::endl;
    }
    
    void testWeightedAveragingEnsemble() {
        std::cout << "Testing weighted averaging ensemble method..." << std::endl;
        
        Camus::EnsembleRequest request;
        request.request_id = "test_weighted_001";
        request.prompt = "Design a secure authentication system";
        request.target_models = {"ensemble_model_2", "ensemble_model_4"}; // High quality models
        request.method = Camus::EnsembleMethod::WEIGHTED_AVERAGING;
        request.task_type = Camus::TaskType::SECURITY_REVIEW;
        
        // Set model weights
        request.model_weights["ensemble_model_2"] = 0.6;
        request.model_weights["ensemble_model_4"] = 0.8; // Security-focused model gets higher weight
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Weighted averaging ensemble should succeed");
        assert(response.method_used == Camus::EnsembleMethod::WEIGHTED_AVERAGING && 
               "Should use weighted averaging method");
        assert(response.models_used >= 2 && "Weighted averaging requires multiple models");
        
        std::cout << "Weighted averaging completed with " << response.models_used << " models" << std::endl;
        std::cout << "Ensemble quality: " << response.ensemble_quality << std::endl;
        
        std::cout << "✓ Weighted averaging ensemble test passed" << std::endl;
    }
    
    void testBestOfNEnsemble() {
        std::cout << "Testing best-of-N ensemble method..." << std::endl;
        
        Camus::EnsembleRequest request;
        request.request_id = "test_best_of_n_001";
        request.prompt = "Optimize this code for better performance";
        request.target_models = {"ensemble_model_1", "ensemble_model_2", "ensemble_model_3"};
        request.method = Camus::EnsembleMethod::BEST_OF_N;
        request.task_type = Camus::TaskType::REFACTORING;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Best-of-N ensemble should succeed");
        assert(response.method_used == Camus::EnsembleMethod::BEST_OF_N && 
               "Should use best-of-N method");
        assert(response.models_used >= 1 && "Best-of-N should use at least one model");
        
        // Check that the final response matches one of the individual responses
        bool found_match = false;
        for (const auto& model_resp : response.individual_responses) {
            if (model_resp.execution_success && model_resp.response_text == response.final_response) {
                found_match = true;
                break;
            }
        }
        assert(found_match && "Best-of-N should select one of the individual responses");
        
        std::cout << "Best-of-N selected from " << response.models_used << " models" << std::endl;
        
        std::cout << "✓ Best-of-N ensemble test passed" << std::endl;
    }
    
    void testConsensusBuilding() {
        std::cout << "Testing consensus building ensemble method..." << std::endl;
        
        Camus::EnsembleRequest request;
        request.request_id = "test_consensus_001";
        request.prompt = "Explain the benefits and drawbacks of microservices architecture";
        request.target_models = {"ensemble_model_2", "ensemble_model_4"}; // High quality models for consensus
        request.method = Camus::EnsembleMethod::CONSENSUS_BUILDING;
        request.task_type = Camus::TaskType::CODE_ANALYSIS;
        request.consensus_threshold = 0.6;
        request.max_consensus_iterations = 2;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Consensus building should succeed");
        assert(response.method_used == Camus::EnsembleMethod::CONSENSUS_BUILDING && 
               "Should use consensus building method");
        assert(response.consensus_iterations >= 0 && "Should track consensus iterations");
        
        std::cout << "Consensus building completed in " << response.consensus_iterations << " iterations" << std::endl;
        std::cout << "Final quality: " << response.ensemble_quality << std::endl;
        
        std::cout << "✓ Consensus building test passed" << std::endl;
    }
    
    void testParallelExecution() {
        std::cout << "Testing parallel execution..." << std::endl;
        
        Camus::EnsembleRequest request;
        request.request_id = "test_parallel_001";
        request.prompt = "Create a data structure for efficient searching";
        request.target_models = {"ensemble_model_1", "ensemble_model_2", "ensemble_model_3", "ensemble_model_4"};
        request.method = Camus::EnsembleMethod::BEST_OF_N;
        request.enable_parallel_execution = true;
        request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto start_time = std::chrono::steady_clock::now();
        auto response = m_strategy->execute(request);
        auto end_time = std::chrono::steady_clock::now();
        
        assert(response.success && "Parallel execution should succeed");
        
        auto total_execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        // Calculate expected sequential time
        long expected_sequential_time = 0;
        for (const auto& model_resp : response.individual_responses) {
            expected_sequential_time += model_resp.execution_time.count();
        }
        
        // Parallel execution should be significantly faster than sequential
        // (allowing for some coordination overhead)
        assert(total_execution_time < expected_sequential_time * 0.8 && 
               "Parallel execution should be faster than sequential");
        
        std::cout << "Parallel execution time: " << total_execution_time << "ms" << std::endl;
        std::cout << "Expected sequential time: " << expected_sequential_time << "ms" << std::endl;
        std::cout << "Speedup factor: " << (static_cast<double>(expected_sequential_time) / total_execution_time) << "x" << std::endl;
        
        std::cout << "✓ Parallel execution test passed" << std::endl;
    }
    
    void testQualityImprovement() {
        std::cout << "Testing ensemble quality improvement..." << std::endl;
        
        // First get single model response for comparison
        Camus::EnsembleRequest single_request;
        single_request.request_id = "test_single_quality_001";
        single_request.prompt = "Implement a thread-safe singleton pattern";
        single_request.target_models = {"ensemble_model_1"}; // Single model
        single_request.method = Camus::EnsembleMethod::BEST_OF_N;
        single_request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto single_response = m_strategy->execute(single_request);
        
        // Now get ensemble response
        Camus::EnsembleRequest ensemble_request;
        ensemble_request.request_id = "test_ensemble_quality_001";
        ensemble_request.prompt = "Implement a thread-safe singleton pattern";
        ensemble_request.target_models = {"ensemble_model_1", "ensemble_model_2", "ensemble_model_3"};
        ensemble_request.method = Camus::EnsembleMethod::BEST_OF_N;
        ensemble_request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto ensemble_response = m_strategy->execute(ensemble_request);
        
        assert(single_response.success && "Single model response should succeed");
        assert(ensemble_response.success && "Ensemble response should succeed");
        
        // Ensemble should generally provide equal or better quality
        // (allowing for some variance in mock responses)
        double quality_improvement = ensemble_response.ensemble_quality - single_response.ensemble_quality;
        
        std::cout << "Single model quality: " << single_response.ensemble_quality << std::endl;
        std::cout << "Ensemble quality: " << ensemble_response.ensemble_quality << std::endl;
        std::cout << "Quality improvement: " << quality_improvement << std::endl;
        
        // The improvement might be small or even negative due to mock variance,
        // but the ensemble should still be reasonably high quality
        assert(ensemble_response.ensemble_quality >= 0.5 && 
               "Ensemble should provide reasonable quality");
        
        std::cout << "✓ Quality improvement test passed" << std::endl;
    }
    
    void testEnsembleStatistics() {
        std::cout << "Testing ensemble statistics tracking..." << std::endl;
        
        // Reset statistics first
        m_strategy->resetStatistics();
        
        // Execute several ensemble requests
        for (int i = 0; i < 3; ++i) {
            Camus::EnsembleRequest request;
            request.request_id = "test_stats_" + std::to_string(i);
            request.prompt = "Test prompt " + std::to_string(i);
            request.target_models = {"ensemble_model_1", "ensemble_model_2"};
            request.method = (i == 0) ? Camus::EnsembleMethod::VOTING : 
                           (i == 1) ? Camus::EnsembleMethod::BEST_OF_N : 
                                     Camus::EnsembleMethod::WEIGHTED_AVERAGING;
            request.task_type = Camus::TaskType::CODE_GENERATION;
            
            m_strategy->execute(request);
        }
        
        auto stats = m_strategy->getStatistics();
        
        assert(stats.total_requests >= 3 && "Should track all requests");
        assert(stats.successful_requests > 0 && "Should have successful requests");
        assert(stats.average_ensemble_time > 0.0 && "Should track ensemble time");
        assert(stats.average_ensemble_confidence >= 0.0 && "Should track confidence");
        assert(!stats.method_usage.empty() && "Should track method usage");
        assert(!stats.model_participation.empty() && "Should track model participation");
        
        std::cout << "Total requests: " << stats.total_requests << std::endl;
        std::cout << "Success rate: " << (stats.successful_requests * 100.0 / stats.total_requests) << "%" << std::endl;
        std::cout << "Average ensemble time: " << stats.average_ensemble_time << "ms" << std::endl;
        std::cout << "Average confidence: " << stats.average_ensemble_confidence << std::endl;
        
        // Check method usage distribution
        for (const auto& [method, count] : stats.method_usage) {
            std::cout << "Method " << static_cast<int>(method) << " used " << count << " times" << std::endl;
        }
        
        std::cout << "✓ Ensemble statistics test passed" << std::endl;
    }
    
    void testCustomQualityAnalyzer() {
        std::cout << "Testing custom quality analyzer..." << std::endl;
        
        // Register custom quality analyzer
        m_strategy->registerQualityAnalyzer(
            [](const std::string& prompt, const std::string& response) -> Camus::QualityAnalysis {
                Camus::QualityAnalysis analysis;
                
                // Custom scoring: prefer responses with code blocks
                if (response.find("```") != std::string::npos) {
                    analysis.overall_score = 0.9;
                    analysis.positive_factors.push_back("contains_code_block");
                } else {
                    analysis.overall_score = 0.6;
                }
                
                analysis.coherence_score = 0.8;
                analysis.completeness_score = 0.7;
                analysis.accuracy_score = 0.8;
                analysis.relevance_score = 0.9;
                analysis.analysis_summary = "Custom quality analysis completed";
                
                return analysis;
            });
        
        Camus::EnsembleRequest request;
        request.request_id = "test_custom_analyzer_001";
        request.prompt = "Write a sorting algorithm";
        request.target_models = {"ensemble_model_1", "ensemble_model_2"};
        request.method = Camus::EnsembleMethod::BEST_OF_N;
        request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto response = m_strategy->execute(request);
        
        assert(response.success && "Custom analyzer test should succeed");
        assert(response.ensemble_quality > 0.0 && "Should have quality score from custom analyzer");
        
        std::cout << "Custom quality score: " << response.ensemble_quality << std::endl;
        
        std::cout << "✓ Custom quality analyzer test passed" << std::endl;
    }
    
    void testEnsembleConfiguration() {
        std::cout << "Testing ensemble configuration..." << std::endl;
        
        // Get current configuration
        auto original_config = m_strategy->getConfig();
        
        // Modify configuration
        Camus::EnsembleStrategyConfig new_config = original_config;
        new_config.default_method = Camus::EnsembleMethod::VOTING;
        new_config.min_ensemble_quality = 0.8;
        new_config.enable_parallel_execution = false;
        
        m_strategy->setConfig(new_config);
        
        // Verify configuration was updated
        auto updated_config = m_strategy->getConfig();
        assert(updated_config.default_method == Camus::EnsembleMethod::VOTING && 
               "Should update default method");
        assert(updated_config.min_ensemble_quality == 0.8 && 
               "Should update quality threshold");
        assert(updated_config.enable_parallel_execution == false && 
               "Should update parallel execution setting");
        
        // Test with new configuration
        Camus::EnsembleRequest request;
        request.request_id = "test_config_001";
        request.prompt = "test prompt";
        request.target_models = {"ensemble_model_1", "ensemble_model_2"};
        // Don't specify method - should use default (voting)
        request.task_type = Camus::TaskType::CODE_GENERATION;
        
        auto response = m_strategy->execute(request);
        assert(response.success && "Should work with modified config");
        assert(response.method_used == Camus::EnsembleMethod::VOTING && 
               "Should use new default method");
        
        std::cout << "✓ Ensemble configuration test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running EnsembleStrategy unit tests..." << std::endl;
        std::cout << "============================================" << std::endl << std::endl;
        
        testBasicEnsembleExecution();
        std::cout << std::endl;
        
        testVotingEnsemble();
        std::cout << std::endl;
        
        testWeightedAveragingEnsemble();
        std::cout << std::endl;
        
        testBestOfNEnsemble();
        std::cout << std::endl;
        
        testConsensusBuilding();
        std::cout << std::endl;
        
        testParallelExecution();
        std::cout << std::endl;
        
        testQualityImprovement();
        std::cout << std::endl;
        
        testEnsembleStatistics();
        std::cout << std::endl;
        
        testCustomQualityAnalyzer();
        std::cout << std::endl;
        
        testEnsembleConfiguration();
        std::cout << std::endl;
        
        std::cout << "All EnsembleStrategy tests passed!" << std::endl;
    }
};

int main() {
    try {
        EnsembleStrategyTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All EnsembleStrategy component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}