// =================================================================
// include/Camus/EnsembleStrategy.hpp
// =================================================================
// Ensemble strategy for multi-model decision making and response combination.

#pragma once

#include "Camus/ModelRegistry.hpp"
#include "Camus/TaskClassifier.hpp"
#include "Camus/ModelCapabilities.hpp"
#include "Camus/LlmInteraction.hpp"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <future>

namespace Camus {

/**
 * @brief Ensemble method type
 */
enum class EnsembleMethod {
    VOTING,                 ///< Majority consensus on decisions
    WEIGHTED_AVERAGING,     ///< Models weighted by confidence/quality
    BEST_OF_N,             ///< Select highest quality response
    CONSENSUS_BUILDING,     ///< Iterative refinement through multiple models
    HYBRID                 ///< Combination of multiple methods
};

/**
 * @brief Individual model response for ensemble
 */
struct ModelResponse {
    std::string model_name;                       ///< Model that generated response
    std::string response_text;                    ///< Generated response
    double confidence_score = 0.0;                ///< Model's confidence (0-1)
    double quality_score = 0.0;                   ///< Response quality score (0-1)
    std::chrono::milliseconds execution_time{0}; ///< Time to generate response
    size_t tokens_generated = 0;                  ///< Number of tokens generated
    std::unordered_map<std::string, double> metrics; ///< Additional metrics
    std::vector<std::string> analysis_tags;      ///< Response analysis tags
    bool execution_success = true;                ///< Whether execution succeeded
    std::string error_message;                    ///< Error message if failed
};

/**
 * @brief Ensemble request structure
 */
struct EnsembleRequest {
    std::string request_id;                       ///< Unique request identifier
    std::string prompt;                           ///< User prompt
    std::string context;                          ///< Additional context
    TaskType task_type = TaskType::UNKNOWN;       ///< Classified task type
    std::vector<std::string> target_models;      ///< Models to include in ensemble
    EnsembleMethod method = EnsembleMethod::BEST_OF_N; ///< Ensemble method to use
    int max_tokens = 2048;                        ///< Maximum output tokens
    double temperature = 0.7;                     ///< Sampling temperature
    std::chrono::milliseconds timeout{60000};    ///< Request timeout
    std::unordered_map<std::string, double> model_weights; ///< Model-specific weights
    double consensus_threshold = 0.7;             ///< Threshold for consensus methods
    size_t max_consensus_iterations = 3;          ///< Max iterations for consensus building
    std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
    bool enable_parallel_execution = true;        ///< Execute models in parallel
    bool enable_early_stopping = false;           ///< Stop early if confidence threshold met
    double early_stopping_threshold = 0.9;       ///< Confidence threshold for early stopping
};

/**
 * @brief Ensemble response structure
 */
struct EnsembleResponse {
    std::string request_id;                       ///< Original request ID
    std::string final_response;                   ///< Final ensemble response
    bool success = false;                         ///< Whether ensemble succeeded
    std::string error_message;                    ///< Error message if failed
    EnsembleMethod method_used;                   ///< Ensemble method used
    std::vector<ModelResponse> individual_responses; ///< All model responses
    double ensemble_confidence = 0.0;             ///< Overall ensemble confidence
    double ensemble_quality = 0.0;                ///< Overall ensemble quality
    std::chrono::milliseconds total_time{0};     ///< Total processing time
    std::chrono::milliseconds coordination_time{0}; ///< Time for ensemble coordination
    size_t models_used = 0;                       ///< Number of models that responded
    size_t consensus_iterations = 0;              ///< Iterations used in consensus building
    std::unordered_map<std::string, double> method_scores; ///< Scores from different methods
    std::vector<std::string> decision_factors;   ///< Factors that influenced final decision
    std::unordered_map<std::string, std::string> debug_info; ///< Debug information
    bool early_stopped = false;                   ///< Whether early stopping was triggered
};

/**
 * @brief Voting configuration for ensemble decisions
 */
struct VotingConfig {
    double weight_threshold = 0.5;               ///< Minimum weight for voting
    bool enable_weighted_voting = true;          ///< Use model weights in voting
    bool require_majority = true;                ///< Require majority for decision
    std::unordered_map<std::string, double> model_voting_weights; ///< Model voting weights
    std::vector<std::string> decision_criteria;  ///< Criteria for voting decisions
};

/**
 * @brief Weighted averaging configuration
 */
struct WeightedAveragingConfig {
    std::unordered_map<std::string, double> base_weights; ///< Base model weights
    bool enable_dynamic_weighting = true;        ///< Adjust weights based on performance
    bool enable_confidence_weighting = true;     ///< Weight by model confidence
    bool enable_quality_weighting = true;        ///< Weight by response quality
    double min_weight = 0.1;                     ///< Minimum weight for any model
    double max_weight = 0.8;                     ///< Maximum weight for any model
    std::string combination_method = "weighted_mean"; ///< How to combine responses
};

/**
 * @brief Best-of-N selection configuration
 */
struct BestOfNConfig {
    std::string selection_criterion = "quality"; ///< "quality", "confidence", "composite"
    std::vector<std::string> quality_metrics;    ///< Metrics to consider for quality
    std::unordered_map<std::string, double> metric_weights; ///< Weights for different metrics
    bool enable_diversity_bonus = true;          ///< Bonus for diverse responses
    double diversity_weight = 0.1;               ///< Weight for diversity in selection
    size_t min_candidates = 2;                   ///< Minimum candidates required
};

/**
 * @brief Consensus building configuration
 */
struct ConsensusConfig {
    size_t max_iterations = 3;                   ///< Maximum consensus iterations
    double convergence_threshold = 0.05;         ///< Threshold for convergence
    std::string aggregation_method = "iterative_refinement"; ///< Consensus method
    bool enable_mediator_model = true;           ///< Use dedicated mediator model
    std::string mediator_model;                  ///< Model to use for mediation
    double consensus_weight = 0.7;               ///< Weight for consensus vs individual
    std::vector<std::string> refinement_prompts; ///< Prompts for refinement iterations
};

/**
 * @brief Configuration for EnsembleStrategy
 */
struct EnsembleStrategyConfig {
    EnsembleMethod default_method = EnsembleMethod::BEST_OF_N; ///< Default ensemble method
    bool enable_parallel_execution = true;       ///< Enable parallel model execution
    bool enable_performance_monitoring = true;   ///< Enable performance tracking
    std::chrono::milliseconds default_timeout{60000}; ///< Default timeout for ensemble
    size_t max_concurrent_models = 5;            ///< Maximum models to run concurrently
    
    VotingConfig voting_config;                  ///< Voting method configuration
    WeightedAveragingConfig weighted_config;     ///< Weighted averaging configuration
    BestOfNConfig best_of_n_config;             ///< Best-of-N selection configuration
    ConsensusConfig consensus_config;            ///< Consensus building configuration
    
    std::unordered_map<TaskType, EnsembleMethod> task_method_preferences; ///< Method per task
    std::unordered_map<std::string, double> model_reliability_scores; ///< Model reliability
    
    double min_ensemble_quality = 0.3;           ///< Minimum acceptable ensemble quality
    double quality_improvement_threshold = 0.15; ///< Required improvement over single model
    bool enable_fallback_to_single = true;       ///< Fallback to best single model
    bool enable_result_caching = true;           ///< Cache ensemble results
};

/**
 * @brief Performance statistics for ensemble strategy
 */
struct EnsembleStatistics {
    size_t total_requests = 0;                   ///< Total ensemble requests
    size_t successful_requests = 0;              ///< Successful ensemble requests
    size_t failed_requests = 0;                  ///< Failed ensemble requests
    size_t early_stopped_requests = 0;           ///< Early stopped requests
    double average_ensemble_time = 0.0;          ///< Average ensemble processing time
    double average_coordination_time = 0.0;      ///< Average coordination overhead
    double average_quality_improvement = 0.0;    ///< Average quality improvement vs single
    double average_ensemble_confidence = 0.0;    ///< Average ensemble confidence
    std::unordered_map<EnsembleMethod, size_t> method_usage; ///< Usage by method
    std::unordered_map<std::string, size_t> model_participation; ///< Model participation counts
    std::unordered_map<std::string, double> model_contribution_scores; ///< Model contributions
    std::unordered_map<TaskType, double> task_quality_improvements; ///< Improvement by task
    std::chrono::system_clock::time_point last_reset; ///< Last statistics reset
};

/**
 * @brief Quality analysis result for response evaluation
 */
struct QualityAnalysis {
    double overall_score = 0.0;                  ///< Overall quality score (0-1)
    std::unordered_map<std::string, double> component_scores; ///< Individual quality components
    std::vector<std::string> positive_factors;   ///< Factors that improved quality
    std::vector<std::string> negative_factors;   ///< Factors that reduced quality
    double coherence_score = 0.0;               ///< Response coherence (0-1)
    double completeness_score = 0.0;            ///< Response completeness (0-1)
    double accuracy_score = 0.0;                ///< Response accuracy (0-1)
    double relevance_score = 0.0;               ///< Response relevance (0-1)
    std::string analysis_summary;               ///< Human-readable analysis summary
};

/**
 * @brief Ensemble strategy for multi-model decision making
 * 
 * Combines outputs from multiple models using various ensemble methods
 * to improve response quality, reliability, and confidence.
 */
class EnsembleStrategy {
public:
    /**
     * @brief Constructor
     * @param registry Model registry reference
     * @param config Ensemble strategy configuration
     */
    EnsembleStrategy(ModelRegistry& registry, 
                    const EnsembleStrategyConfig& config = EnsembleStrategyConfig());
    
    /**
     * @brief Destructor
     */
    virtual ~EnsembleStrategy() = default;
    
    /**
     * @brief Execute ensemble request
     * @param request Ensemble request
     * @return Ensemble response
     */
    virtual EnsembleResponse execute(const EnsembleRequest& request);
    
    /**
     * @brief Get strategy configuration
     * @return Current configuration
     */
    virtual EnsembleStrategyConfig getConfig() const;
    
    /**
     * @brief Set strategy configuration
     * @param config New configuration
     */
    virtual void setConfig(const EnsembleStrategyConfig& config);
    
    /**
     * @brief Get strategy statistics
     * @return Current statistics
     */
    virtual EnsembleStatistics getStatistics() const;
    
    /**
     * @brief Reset statistics
     */
    virtual void resetStatistics();
    
    /**
     * @brief Register custom quality analyzer
     * @param analyzer Function to analyze response quality
     */
    virtual void registerQualityAnalyzer(
        std::function<QualityAnalysis(const std::string&, const std::string&)> analyzer);
    
    /**
     * @brief Register custom response combiner
     * @param combiner Function to combine multiple responses
     */
    virtual void registerResponseCombiner(
        std::function<std::string(const std::vector<ModelResponse>&)> combiner);

protected:
    /**
     * @brief Execute voting ensemble method
     * @param request Ensemble request
     * @param responses Individual model responses
     * @return Final ensemble response
     */
    virtual std::string executeVoting(const EnsembleRequest& request,
                                     const std::vector<ModelResponse>& responses);
    
    /**
     * @brief Execute weighted averaging ensemble method
     * @param request Ensemble request
     * @param responses Individual model responses
     * @return Final ensemble response
     */
    virtual std::string executeWeightedAveraging(const EnsembleRequest& request,
                                                 const std::vector<ModelResponse>& responses);
    
    /**
     * @brief Execute best-of-N selection ensemble method
     * @param request Ensemble request
     * @param responses Individual model responses
     * @return Final ensemble response
     */
    virtual std::string executeBestOfN(const EnsembleRequest& request,
                                      const std::vector<ModelResponse>& responses);
    
    /**
     * @brief Execute consensus building ensemble method
     * @param request Ensemble request
     * @param responses Individual model responses
     * @return Final ensemble response
     */
    virtual std::string executeConsensusBuilding(const EnsembleRequest& request,
                                                 const std::vector<ModelResponse>& responses);
    
    /**
     * @brief Execute individual model requests
     * @param request Ensemble request
     * @return Vector of model responses
     */
    virtual std::vector<ModelResponse> executeModels(const EnsembleRequest& request);
    
    /**
     * @brief Execute single model request
     * @param model_name Model to execute
     * @param request Ensemble request
     * @return Model response
     */
    virtual ModelResponse executeSingleModel(const std::string& model_name,
                                            const EnsembleRequest& request);
    
    /**
     * @brief Analyze response quality
     * @param prompt Original prompt
     * @param response Generated response
     * @param task_type Task type for context
     * @return Quality analysis result
     */
    virtual QualityAnalysis analyzeQuality(const std::string& prompt,
                                          const std::string& response,
                                          TaskType task_type);
    
    /**
     * @brief Calculate ensemble confidence
     * @param responses Individual model responses
     * @param method Ensemble method used
     * @return Ensemble confidence score (0-1)
     */
    virtual double calculateEnsembleConfidence(const std::vector<ModelResponse>& responses,
                                              EnsembleMethod method);
    
    /**
     * @brief Calculate model weights for weighted averaging
     * @param responses Individual model responses
     * @param request Original request
     * @return Map of model weights
     */
    virtual std::unordered_map<std::string, double> calculateModelWeights(
        const std::vector<ModelResponse>& responses,
        const EnsembleRequest& request);
    
    /**
     * @brief Detect consensus among responses
     * @param responses Model responses
     * @param threshold Consensus threshold
     * @return True if consensus detected
     */
    virtual bool detectConsensus(const std::vector<ModelResponse>& responses,
                                double threshold);
    
    /**
     * @brief Combine responses using weighted averaging
     * @param responses Model responses
     * @param weights Model weights
     * @return Combined response text
     */
    virtual std::string combineResponsesWeighted(const std::vector<ModelResponse>& responses,
                                                 const std::unordered_map<std::string, double>& weights);
    
    /**
     * @brief Calculate response similarity
     * @param response1 First response
     * @param response2 Second response
     * @return Similarity score (0-1)
     */
    virtual double calculateResponseSimilarity(const std::string& response1,
                                              const std::string& response2);
    
    /**
     * @brief Update performance statistics
     * @param request Original request
     * @param response Ensemble response
     */
    virtual void updateStatistics(const EnsembleRequest& request,
                                 const EnsembleResponse& response);

private:
    ModelRegistry& m_registry;
    EnsembleStrategyConfig m_config;
    EnsembleStatistics m_statistics;
    
    std::function<QualityAnalysis(const std::string&, const std::string&)> m_quality_analyzer;
    std::function<std::string(const std::vector<ModelResponse>&)> m_response_combiner;
    
    mutable std::mutex m_stats_mutex;
    mutable std::mutex m_config_mutex;
    
    /**
     * @brief Initialize default quality analyzer
     */
    void initializeDefaultAnalyzer();
    
    /**
     * @brief Initialize default response combiner
     */
    void initializeDefaultCombiner();
    
    /**
     * @brief Get ensemble method for request
     * @param request Ensemble request
     * @return Appropriate ensemble method
     */
    EnsembleMethod selectEnsembleMethod(const EnsembleRequest& request);
    
    /**
     * @brief Filter and validate target models
     * @param target_models Requested models
     * @return Available and healthy models
     */
    std::vector<std::string> validateTargetModels(const std::vector<std::string>& target_models);
    
    /**
     * @brief Default quality analyzer implementation
     * @param prompt Original prompt
     * @param response Generated response
     * @return Quality analysis
     */
    QualityAnalysis defaultQualityAnalyzer(const std::string& prompt, const std::string& response);
    
    /**
     * @brief Default response combiner implementation
     * @param responses Model responses
     * @return Combined response
     */
    std::string defaultResponseCombiner(const std::vector<ModelResponse>& responses);
    
    /**
     * @brief Extract decision factors from ensemble process
     * @param method Ensemble method used
     * @param responses Model responses
     * @param final_response Final response
     * @return Decision factors
     */
    std::vector<std::string> extractDecisionFactors(EnsembleMethod method,
                                                    const std::vector<ModelResponse>& responses,
                                                    const std::string& final_response);
};

/**
 * @brief Convert ensemble method to string representation
 * @param method Ensemble method
 * @return String representation
 */
std::string ensembleMethodToString(EnsembleMethod method);

/**
 * @brief Convert string to ensemble method
 * @param str String representation
 * @return Ensemble method
 */
EnsembleMethod stringToEnsembleMethod(const std::string& str);

} // namespace Camus