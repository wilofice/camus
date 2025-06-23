// =================================================================
// src/Camus/EnsembleStrategy.cpp
// =================================================================
// Implementation of ensemble strategy for multi-model decision making.

#include "Camus/EnsembleStrategy.hpp"
#include "Camus/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <random>
#include <numeric>
#include <future>
#include <thread>
#include <cmath>
#include <unordered_set>

namespace Camus {

EnsembleStrategy::EnsembleStrategy(ModelRegistry& registry, 
                                 const EnsembleStrategyConfig& config)
    : m_registry(registry), m_config(config) {
    
    // Initialize default analyzers and combiners
    initializeDefaultAnalyzer();
    initializeDefaultCombiner();
    
    // Initialize statistics
    m_statistics.last_reset = std::chrono::system_clock::now();
    
    Logger::getInstance().info("EnsembleStrategy", "Strategy initialized successfully");
}

EnsembleResponse EnsembleStrategy::execute(const EnsembleRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    EnsembleResponse response;
    response.request_id = request.request_id;
    response.method_used = selectEnsembleMethod(request);
    
    Logger::getInstance().debug("EnsembleStrategy", 
        "Executing ensemble request: " + request.request_id + 
        " with method: " + ensembleMethodToString(response.method_used));
    
    try {
        // Validate and filter target models
        std::vector<std::string> valid_models = validateTargetModels(request.target_models);
        if (valid_models.empty()) {
            throw std::runtime_error("No valid models available for ensemble");
        }
        
        // Execute individual model requests
        auto coordination_start = std::chrono::steady_clock::now();
        
        EnsembleRequest modified_request = request;
        modified_request.target_models = valid_models;
        
        std::vector<ModelResponse> individual_responses = executeModels(modified_request);
        
        auto coordination_end = std::chrono::steady_clock::now();
        response.coordination_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            coordination_end - coordination_start);
        
        // Filter successful responses
        std::vector<ModelResponse> successful_responses;
        for (const auto& resp : individual_responses) {
            if (resp.execution_success && !resp.response_text.empty()) {
                successful_responses.push_back(resp);
            }
        }
        
        if (successful_responses.empty()) {
            throw std::runtime_error("No models produced successful responses");
        }
        
        response.individual_responses = individual_responses;
        response.models_used = successful_responses.size();
        
        // Apply ensemble method
        std::string final_response;
        switch (response.method_used) {
            case EnsembleMethod::VOTING:
                final_response = executeVoting(modified_request, successful_responses);
                break;
            case EnsembleMethod::WEIGHTED_AVERAGING:
                final_response = executeWeightedAveraging(modified_request, successful_responses);
                break;
            case EnsembleMethod::BEST_OF_N:
                final_response = executeBestOfN(modified_request, successful_responses);
                break;
            case EnsembleMethod::CONSENSUS_BUILDING:
                final_response = executeConsensusBuilding(modified_request, successful_responses);
                break;
            case EnsembleMethod::HYBRID:
                // Use best-of-N as default for hybrid
                final_response = executeBestOfN(modified_request, successful_responses);
                break;
        }
        
        response.final_response = final_response;
        response.success = true;
        
        // Calculate ensemble metrics
        response.ensemble_confidence = calculateEnsembleConfidence(successful_responses, response.method_used);
        
        // Analyze ensemble quality
        auto quality_analysis = analyzeQuality(request.prompt, final_response, request.task_type);
        response.ensemble_quality = quality_analysis.overall_score;
        
        // Extract decision factors
        response.decision_factors = extractDecisionFactors(response.method_used, 
                                                          successful_responses, final_response);
        
        // Add debug information
        response.debug_info["method_used"] = ensembleMethodToString(response.method_used);
        response.debug_info["models_attempted"] = std::to_string(valid_models.size());
        response.debug_info["models_successful"] = std::to_string(successful_responses.size());
        response.debug_info["ensemble_confidence"] = std::to_string(response.ensemble_confidence);
        response.debug_info["ensemble_quality"] = std::to_string(response.ensemble_quality);
        
        Logger::getInstance().info("EnsembleStrategy", 
            "Ensemble completed successfully: " + request.request_id + 
            " (models: " + std::to_string(response.models_used) + 
            ", confidence: " + std::to_string(response.ensemble_confidence) + 
            ", quality: " + std::to_string(response.ensemble_quality) + ")");
        
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = e.what();
        
        Logger::getInstance().error("EnsembleStrategy", 
            "Ensemble execution failed: " + response.error_message + 
            " (request: " + request.request_id + ")");
        
        // Fallback to single best model if enabled
        if (m_config.enable_fallback_to_single && !response.individual_responses.empty()) {
            auto best_response = std::max_element(response.individual_responses.begin(),
                                                response.individual_responses.end(),
                                                [](const ModelResponse& a, const ModelResponse& b) {
                                                    return a.quality_score < b.quality_score;
                                                });
            
            if (best_response != response.individual_responses.end() && best_response->execution_success) {
                response.final_response = best_response->response_text;
                response.success = true;
                response.error_message = "";
                response.decision_factors.push_back("fallback_to_single_best_model");
                
                Logger::getInstance().info("EnsembleStrategy", 
                    "Fallback to single model successful: " + best_response->model_name);
            }
        }
    }
    
    // Calculate total time
    auto end_time = std::chrono::steady_clock::now();
    response.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    // Update statistics
    updateStatistics(request, response);
    
    return response;
}

std::vector<ModelResponse> EnsembleStrategy::executeModels(const EnsembleRequest& request) {
    std::vector<ModelResponse> responses;
    
    if (request.enable_parallel_execution && m_config.enable_parallel_execution) {
        // Parallel execution
        std::vector<std::future<ModelResponse>> futures;
        
        for (const auto& model_name : request.target_models) {
            futures.push_back(std::async(std::launch::async,
                [this, model_name, request]() {
                    return executeSingleModel(model_name, request);
                }));
        }
        
        // Collect results
        for (auto& future : futures) {
            try {
                responses.push_back(future.get());
            } catch (const std::exception& e) {
                Logger::getInstance().warning("EnsembleStrategy", 
                    "Model execution failed: " + std::string(e.what()));
            }
        }
    } else {
        // Sequential execution
        for (const auto& model_name : request.target_models) {
            try {
                responses.push_back(executeSingleModel(model_name, request));
            } catch (const std::exception& e) {
                Logger::getInstance().warning("EnsembleStrategy", 
                    "Model execution failed: " + std::string(e.what()));
            }
        }
    }
    
    return responses;
}

ModelResponse EnsembleStrategy::executeSingleModel(const std::string& model_name,
                                                  const EnsembleRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    ModelResponse response;
    response.model_name = model_name;
    
    try {
        auto model = m_registry.getModel(model_name);
        if (!model) {
            throw std::runtime_error("Model not available: " + model_name);
        }
        
        // Execute the model
        std::string model_response = model->getCompletion(request.prompt);
        
        auto end_time = std::chrono::steady_clock::now();
        response.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        response.response_text = model_response;
        response.execution_success = true;
        
        // Estimate tokens generated (rough approximation)
        response.tokens_generated = model_response.length() / 4;
        
        // Calculate quality and confidence scores
        auto quality_analysis = analyzeQuality(request.prompt, model_response, request.task_type);
        response.quality_score = quality_analysis.overall_score;
        response.confidence_score = std::min(1.0, quality_analysis.overall_score + 0.1);
        
        // Add analysis tags based on quality analysis
        if (quality_analysis.coherence_score > 0.8) response.analysis_tags.push_back("coherent");
        if (quality_analysis.completeness_score > 0.8) response.analysis_tags.push_back("complete");
        if (quality_analysis.accuracy_score > 0.8) response.analysis_tags.push_back("accurate");
        if (quality_analysis.relevance_score > 0.8) response.analysis_tags.push_back("relevant");
        
        // Store quality components as metrics
        response.metrics["coherence"] = quality_analysis.coherence_score;
        response.metrics["completeness"] = quality_analysis.completeness_score;
        response.metrics["accuracy"] = quality_analysis.accuracy_score;
        response.metrics["relevance"] = quality_analysis.relevance_score;
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        response.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        response.execution_success = false;
        response.error_message = e.what();
        response.quality_score = 0.0;
        response.confidence_score = 0.0;
    }
    
    return response;
}

std::string EnsembleStrategy::executeVoting(const EnsembleRequest& request,
                                           const std::vector<ModelResponse>& responses) {
    
    if (responses.empty()) {
        return "";
    }
    
    if (responses.size() == 1) {
        return responses[0].response_text;
    }
    
    // Simple majority voting based on response similarity
    std::unordered_map<std::string, double> response_votes;
    std::unordered_map<std::string, std::vector<std::string>> response_supporters;
    
    // Group similar responses
    for (const auto& response : responses) {
        bool found_similar = false;
        
        for (auto& [candidate_response, votes] : response_votes) {
            double similarity = calculateResponseSimilarity(response.response_text, candidate_response);
            
            if (similarity > 0.7) { // Threshold for considering responses similar
                // Weight the vote by model quality and confidence
                double vote_weight = 1.0;
                if (m_config.voting_config.enable_weighted_voting) {
                    vote_weight = (response.quality_score + response.confidence_score) / 2.0;
                    
                    // Apply model-specific weights if available
                    auto weight_it = m_config.voting_config.model_voting_weights.find(response.model_name);
                    if (weight_it != m_config.voting_config.model_voting_weights.end()) {
                        vote_weight *= weight_it->second;
                    }
                }
                
                votes += vote_weight;
                response_supporters[candidate_response].push_back(response.model_name);
                found_similar = true;
                break;
            }
        }
        
        if (!found_similar) {
            // New unique response
            double vote_weight = 1.0;
            if (m_config.voting_config.enable_weighted_voting) {
                vote_weight = (response.quality_score + response.confidence_score) / 2.0;
                
                auto weight_it = m_config.voting_config.model_voting_weights.find(response.model_name);
                if (weight_it != m_config.voting_config.model_voting_weights.end()) {
                    vote_weight *= weight_it->second;
                }
            }
            
            response_votes[response.response_text] = vote_weight;
            response_supporters[response.response_text].push_back(response.model_name);
        }
    }
    
    // Find the response with the most votes
    auto winner = std::max_element(response_votes.begin(), response_votes.end(),
                                  [](const auto& a, const auto& b) {
                                      return a.second < b.second;
                                  });
    
    if (winner != response_votes.end()) {
        // Check if majority requirement is met
        if (m_config.voting_config.require_majority) {
            double total_votes = std::accumulate(response_votes.begin(), response_votes.end(), 0.0,
                                               [](double sum, const auto& pair) {
                                                   return sum + pair.second;
                                               });
            
            if (winner->second / total_votes < 0.5) {
                // No majority, fall back to best quality response
                auto best_response = std::max_element(responses.begin(), responses.end(),
                                                    [](const ModelResponse& a, const ModelResponse& b) {
                                                        return a.quality_score < b.quality_score;
                                                    });
                return best_response->response_text;
            }
        }
        
        Logger::getInstance().debug("EnsembleStrategy", 
            "Voting winner selected with " + std::to_string(winner->second) + " votes from " +
            std::to_string(response_supporters[winner->first].size()) + " models");
        
        return winner->first;
    }
    
    // Fallback to highest quality response
    auto best_response = std::max_element(responses.begin(), responses.end(),
                                        [](const ModelResponse& a, const ModelResponse& b) {
                                            return a.quality_score < b.quality_score;
                                        });
    return best_response->response_text;
}

std::string EnsembleStrategy::executeWeightedAveraging(const EnsembleRequest& request,
                                                      const std::vector<ModelResponse>& responses) {
    
    if (responses.empty()) {
        return "";
    }
    
    if (responses.size() == 1) {
        return responses[0].response_text;
    }
    
    // Calculate model weights
    auto weights = calculateModelWeights(responses, request);
    
    // Use custom combiner if available, otherwise use default
    if (m_response_combiner) {
        return m_response_combiner(responses);
    } else {
        return combineResponsesWeighted(responses, weights);
    }
}

std::string EnsembleStrategy::executeBestOfN(const EnsembleRequest& request,
                                            const std::vector<ModelResponse>& responses) {
    
    if (responses.empty()) {
        return "";
    }
    
    if (responses.size() == 1) {
        return responses[0].response_text;
    }
    
    // Score each response based on selection criterion
    std::vector<std::pair<const ModelResponse*, double>> scored_responses;
    
    for (const auto& response : responses) {
        double score = 0.0;
        
        if (m_config.best_of_n_config.selection_criterion == "quality") {
            score = response.quality_score;
        } else if (m_config.best_of_n_config.selection_criterion == "confidence") {
            score = response.confidence_score;
        } else if (m_config.best_of_n_config.selection_criterion == "composite") {
            // Composite score combining multiple factors
            score = 0.4 * response.quality_score + 
                   0.3 * response.confidence_score + 
                   0.2 * (1.0 - response.execution_time.count() / 10000.0) + // Speed bonus
                   0.1 * (response.tokens_generated / 1000.0); // Length consideration
            
            // Apply metric weights if specified
            for (const auto& [metric, weight] : m_config.best_of_n_config.metric_weights) {
                auto metric_it = response.metrics.find(metric);
                if (metric_it != response.metrics.end()) {
                    score += weight * metric_it->second;
                }
            }
        }
        
        // Apply diversity bonus if enabled
        if (m_config.best_of_n_config.enable_diversity_bonus) {
            double diversity_score = 0.0;
            for (const auto& other_response : responses) {
                if (&other_response != &response) {
                    double similarity = calculateResponseSimilarity(response.response_text, 
                                                                   other_response.response_text);
                    diversity_score += (1.0 - similarity);
                }
            }
            diversity_score /= (responses.size() - 1);
            score += m_config.best_of_n_config.diversity_weight * diversity_score;
        }
        
        scored_responses.push_back({&response, score});
    }
    
    // Sort by score (highest first)
    std::sort(scored_responses.begin(), scored_responses.end(),
             [](const auto& a, const auto& b) {
                 return a.second > b.second;
             });
    
    Logger::getInstance().debug("EnsembleStrategy", 
        "Best-of-N selected: " + scored_responses[0].first->model_name + 
        " with score: " + std::to_string(scored_responses[0].second));
    
    return scored_responses[0].first->response_text;
}

std::string EnsembleStrategy::executeConsensusBuilding(const EnsembleRequest& request,
                                                      const std::vector<ModelResponse>& responses) {
    
    if (responses.empty()) {
        return "";
    }
    
    if (responses.size() == 1) {
        return responses[0].response_text;
    }
    
    std::vector<ModelResponse> current_responses = responses;
    size_t iteration = 0;
    
    while (iteration < m_config.consensus_config.max_iterations) {
        // Check for consensus
        if (detectConsensus(current_responses, request.consensus_threshold)) {
            break;
        }
        
        // Refinement iteration
        std::vector<ModelResponse> refined_responses;
        
        // Create refinement prompt
        std::string consensus_prompt = "Based on these responses:\n\n";
        for (size_t i = 0; i < current_responses.size(); ++i) {
            consensus_prompt += "Response " + std::to_string(i + 1) + " from " + 
                              current_responses[i].model_name + ":\n";
            consensus_prompt += current_responses[i].response_text + "\n\n";
        }
        
        consensus_prompt += "Please provide a refined response that synthesizes the best elements from all responses above. ";
        consensus_prompt += "Focus on accuracy, completeness, and clarity.";
        
        // Execute refinement with available models
        EnsembleRequest refinement_request = request;
        refinement_request.prompt = consensus_prompt;
        refinement_request.request_id = request.request_id + "_refinement_" + std::to_string(iteration);
        
        for (const auto& model_name : request.target_models) {
            try {
                auto refined_response = executeSingleModel(model_name, refinement_request);
                if (refined_response.execution_success) {
                    refined_responses.push_back(refined_response);
                }
            } catch (const std::exception& e) {
                Logger::getInstance().warning("EnsembleStrategy", 
                    "Consensus refinement failed for model " + model_name + ": " + e.what());
            }
        }
        
        if (!refined_responses.empty()) {
            current_responses = refined_responses;
        }
        
        iteration++;
    }
    
    // Select best response from final iteration
    auto best_response = std::max_element(current_responses.begin(), current_responses.end(),
                                        [](const ModelResponse& a, const ModelResponse& b) {
                                            return a.quality_score < b.quality_score;
                                        });
    
    Logger::getInstance().debug("EnsembleStrategy", 
        "Consensus building completed after " + std::to_string(iteration) + " iterations");
    
    return best_response->response_text;
}

QualityAnalysis EnsembleStrategy::analyzeQuality(const std::string& prompt,
                                                const std::string& response,
                                                TaskType task_type) {
    
    if (m_quality_analyzer) {
        return m_quality_analyzer(prompt, response);
    } else {
        return defaultQualityAnalyzer(prompt, response);
    }
}

double EnsembleStrategy::calculateEnsembleConfidence(const std::vector<ModelResponse>& responses,
                                                    EnsembleMethod method) {
    
    if (responses.empty()) {
        return 0.0;
    }
    
    double total_confidence = 0.0;
    double total_weight = 0.0;
    
    switch (method) {
        case EnsembleMethod::VOTING: {
            // Confidence based on consensus strength
            std::unordered_map<std::string, double> response_votes;
            for (const auto& response : responses) {
                bool found_similar = false;
                for (auto& [candidate, votes] : response_votes) {
                    if (calculateResponseSimilarity(response.response_text, candidate) > 0.7) {
                        votes += response.confidence_score;
                        found_similar = true;
                        break;
                    }
                }
                if (!found_similar) {
                    response_votes[response.response_text] = response.confidence_score;
                }
            }
            
            double max_votes = 0.0;
            double total_votes = 0.0;
            for (const auto& [resp, votes] : response_votes) {
                max_votes = std::max(max_votes, votes);
                total_votes += votes;
            }
            
            return total_votes > 0 ? max_votes / total_votes : 0.0;
        }
        
        case EnsembleMethod::WEIGHTED_AVERAGING: {
            // Weighted average of individual confidences
            for (const auto& response : responses) {
                double weight = response.quality_score;
                total_confidence += response.confidence_score * weight;
                total_weight += weight;
            }
            break;
        }
        
        case EnsembleMethod::BEST_OF_N: {
            // Confidence of the best response
            auto best_response = std::max_element(responses.begin(), responses.end(),
                                                [](const ModelResponse& a, const ModelResponse& b) {
                                                    return a.quality_score < b.quality_score;
                                                });
            return best_response->confidence_score;
        }
        
        case EnsembleMethod::CONSENSUS_BUILDING: {
            // Average confidence with consensus bonus
            for (const auto& response : responses) {
                total_confidence += response.confidence_score;
            }
            double avg_confidence = total_confidence / responses.size();
            
            // Add consensus bonus
            double similarity_sum = 0.0;
            size_t comparison_count = 0;
            for (size_t i = 0; i < responses.size(); ++i) {
                for (size_t j = i + 1; j < responses.size(); ++j) {
                    similarity_sum += calculateResponseSimilarity(responses[i].response_text,
                                                                responses[j].response_text);
                    comparison_count++;
                }
            }
            
            double consensus_bonus = comparison_count > 0 ? similarity_sum / comparison_count : 0.0;
            return std::min(1.0, avg_confidence + 0.2 * consensus_bonus);
        }
        
        case EnsembleMethod::HYBRID: {
            // Average of all individual confidences
            for (const auto& response : responses) {
                total_confidence += response.confidence_score;
            }
            return total_confidence / responses.size();
        }
    }
    
    return total_weight > 0 ? total_confidence / total_weight : 0.0;
}

std::unordered_map<std::string, double> EnsembleStrategy::calculateModelWeights(
    const std::vector<ModelResponse>& responses,
    const EnsembleRequest& request) {
    
    std::unordered_map<std::string, double> weights;
    double total_weight = 0.0;
    
    for (const auto& response : responses) {
        double weight = 1.0; // Base weight
        
        // Apply base weights from configuration
        auto base_weight_it = m_config.weighted_config.base_weights.find(response.model_name);
        if (base_weight_it != m_config.weighted_config.base_weights.end()) {
            weight = base_weight_it->second;
        }
        
        // Dynamic weighting based on performance
        if (m_config.weighted_config.enable_dynamic_weighting) {
            // Adjust based on execution time (faster = slightly higher weight)
            double time_factor = std::max(0.5, 1.0 - response.execution_time.count() / 10000.0);
            weight *= (0.8 + 0.2 * time_factor);
        }
        
        // Confidence-based weighting
        if (m_config.weighted_config.enable_confidence_weighting) {
            weight *= (0.5 + 0.5 * response.confidence_score);
        }
        
        // Quality-based weighting
        if (m_config.weighted_config.enable_quality_weighting) {
            weight *= (0.3 + 0.7 * response.quality_score);
        }
        
        // Apply min/max constraints
        weight = std::max(m_config.weighted_config.min_weight, 
                         std::min(m_config.weighted_config.max_weight, weight));
        
        weights[response.model_name] = weight;
        total_weight += weight;
    }
    
    // Normalize weights
    if (total_weight > 0) {
        for (auto& [model, weight] : weights) {
            weight /= total_weight;
        }
    }
    
    return weights;
}

bool EnsembleStrategy::detectConsensus(const std::vector<ModelResponse>& responses,
                                      double threshold) {
    
    if (responses.size() < 2) {
        return true;
    }
    
    // Calculate average pairwise similarity
    double similarity_sum = 0.0;
    size_t comparison_count = 0;
    
    for (size_t i = 0; i < responses.size(); ++i) {
        for (size_t j = i + 1; j < responses.size(); ++j) {
            similarity_sum += calculateResponseSimilarity(responses[i].response_text,
                                                        responses[j].response_text);
            comparison_count++;
        }
    }
    
    if (comparison_count == 0) {
        return true;
    }
    
    double average_similarity = similarity_sum / comparison_count;
    return average_similarity >= threshold;
}

std::string EnsembleStrategy::combineResponsesWeighted(
    const std::vector<ModelResponse>& responses,
    const std::unordered_map<std::string, double>& weights) {
    
    if (responses.empty()) {
        return "";
    }
    
    if (responses.size() == 1) {
        return responses[0].response_text;
    }
    
    // For text responses, weighted combination is complex
    // We'll use a sophisticated approach based on sentence-level weighting
    
    std::vector<std::string> all_sentences;
    std::unordered_map<std::string, double> sentence_weights;
    
    // Extract sentences from all responses
    for (const auto& response : responses) {
        auto weight_it = weights.find(response.model_name);
        double model_weight = weight_it != weights.end() ? weight_it->second : 1.0 / responses.size();
        
        // Simple sentence splitting
        std::regex sentence_regex(R"([.!?]+\s*)");
        std::vector<std::string> sentences;
        std::sregex_token_iterator iter(response.response_text.begin(), 
                                       response.response_text.end(), 
                                       sentence_regex, -1);
        std::sregex_token_iterator end;
        
        for (; iter != end; ++iter) {
            std::string sentence = iter->str();
            if (!sentence.empty() && sentence.length() > 5) {
                sentences.push_back(sentence);
            }
        }
        
        // Weight sentences
        for (const auto& sentence : sentences) {
            if (sentence_weights.find(sentence) == sentence_weights.end()) {
                sentence_weights[sentence] = model_weight;
                all_sentences.push_back(sentence);
            } else {
                sentence_weights[sentence] += model_weight;
            }
        }
    }
    
    // Sort sentences by weight and select top ones
    std::sort(all_sentences.begin(), all_sentences.end(),
             [&sentence_weights](const std::string& a, const std::string& b) {
                 return sentence_weights[a] > sentence_weights[b];
             });
    
    // Combine top sentences into final response
    std::string combined_response;
    size_t max_sentences = std::min(all_sentences.size(), static_cast<size_t>(10)); // Limit to 10 sentences
    
    for (size_t i = 0; i < max_sentences; ++i) {
        if (!combined_response.empty()) {
            combined_response += " ";
        }
        combined_response += all_sentences[i];
    }
    
    return combined_response;
}

double EnsembleStrategy::calculateResponseSimilarity(const std::string& response1,
                                                    const std::string& response2) {
    
    if (response1.empty() || response2.empty()) {
        return 0.0;
    }
    
    // Simple word-based similarity using Jaccard index
    std::regex word_regex(R"(\b\w+\b)");
    
    std::unordered_set<std::string> words1, words2;
    
    std::sregex_iterator iter1(response1.begin(), response1.end(), word_regex);
    std::sregex_iterator iter2(response2.begin(), response2.end(), word_regex);
    std::sregex_iterator end;
    
    for (; iter1 != end; ++iter1) {
        std::string word = iter1->str();
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        words1.insert(word);
    }
    
    for (; iter2 != end; ++iter2) {
        std::string word = iter2->str();
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        words2.insert(word);
    }
    
    // Calculate Jaccard similarity
    size_t intersection_size = 0;
    for (const auto& word : words1) {
        if (words2.count(word)) {
            intersection_size++;
        }
    }
    
    size_t union_size = words1.size() + words2.size() - intersection_size;
    
    return union_size > 0 ? static_cast<double>(intersection_size) / union_size : 0.0;
}

void EnsembleStrategy::updateStatistics(const EnsembleRequest& request,
                                       const EnsembleResponse& response) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_statistics.total_requests++;
    
    if (response.success) {
        m_statistics.successful_requests++;
    } else {
        m_statistics.failed_requests++;
    }
    
    if (response.early_stopped) {
        m_statistics.early_stopped_requests++;
    }
    
    // Update averages
    if (m_statistics.total_requests == 1) {
        m_statistics.average_ensemble_time = response.total_time.count();
        m_statistics.average_coordination_time = response.coordination_time.count();
        m_statistics.average_ensemble_confidence = response.ensemble_confidence;
    } else {
        double n = static_cast<double>(m_statistics.total_requests);
        m_statistics.average_ensemble_time = 
            (m_statistics.average_ensemble_time * (n - 1) + response.total_time.count()) / n;
        m_statistics.average_coordination_time = 
            (m_statistics.average_coordination_time * (n - 1) + response.coordination_time.count()) / n;
        m_statistics.average_ensemble_confidence = 
            (m_statistics.average_ensemble_confidence * (n - 1) + response.ensemble_confidence) / n;
    }
    
    // Update method usage
    m_statistics.method_usage[response.method_used]++;
    
    // Update model participation
    for (const auto& model_response : response.individual_responses) {
        m_statistics.model_participation[model_response.model_name]++;
        
        if (model_response.execution_success) {
            if (m_statistics.model_contribution_scores.find(model_response.model_name) == 
                m_statistics.model_contribution_scores.end()) {
                m_statistics.model_contribution_scores[model_response.model_name] = model_response.quality_score;
            } else {
                m_statistics.model_contribution_scores[model_response.model_name] = 
                    (m_statistics.model_contribution_scores[model_response.model_name] + 
                     model_response.quality_score) / 2.0;
            }
        }
    }
    
    // Track quality improvement per task
    if (response.success && response.ensemble_quality > 0) {
        if (m_statistics.task_quality_improvements.find(request.task_type) == 
            m_statistics.task_quality_improvements.end()) {
            m_statistics.task_quality_improvements[request.task_type] = response.ensemble_quality;
        } else {
            m_statistics.task_quality_improvements[request.task_type] = 
                (m_statistics.task_quality_improvements[request.task_type] + response.ensemble_quality) / 2.0;
        }
    }
}

EnsembleStrategyConfig EnsembleStrategy::getConfig() const {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    return m_config;
}

void EnsembleStrategy::setConfig(const EnsembleStrategyConfig& config) {
    std::lock_guard<std::mutex> lock(m_config_mutex);
    m_config = config;
    Logger::getInstance().info("EnsembleStrategy", "Configuration updated");
}

EnsembleStatistics EnsembleStrategy::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_statistics;
}

void EnsembleStrategy::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_statistics = EnsembleStatistics();
    m_statistics.last_reset = std::chrono::system_clock::now();
    Logger::getInstance().info("EnsembleStrategy", "Statistics reset");
}

void EnsembleStrategy::registerQualityAnalyzer(
    std::function<QualityAnalysis(const std::string&, const std::string&)> analyzer) {
    m_quality_analyzer = analyzer;
    Logger::getInstance().info("EnsembleStrategy", "Custom quality analyzer registered");
}

void EnsembleStrategy::registerResponseCombiner(
    std::function<std::string(const std::vector<ModelResponse>&)> combiner) {
    m_response_combiner = combiner;
    Logger::getInstance().info("EnsembleStrategy", "Custom response combiner registered");
}

void EnsembleStrategy::initializeDefaultAnalyzer() {
    m_quality_analyzer = [this](const std::string& prompt, const std::string& response) {
        return defaultQualityAnalyzer(prompt, response);
    };
}

void EnsembleStrategy::initializeDefaultCombiner() {
    m_response_combiner = [this](const std::vector<ModelResponse>& responses) {
        return defaultResponseCombiner(responses);
    };
}

EnsembleMethod EnsembleStrategy::selectEnsembleMethod(const EnsembleRequest& request) {
    // Use specified method if provided
    if (request.method != EnsembleMethod::BEST_OF_N || 
        request.method != m_config.default_method) {
        return request.method;
    }
    
    // Check task-specific preferences
    auto task_pref_it = m_config.task_method_preferences.find(request.task_type);
    if (task_pref_it != m_config.task_method_preferences.end()) {
        return task_pref_it->second;
    }
    
    // Use default method
    return m_config.default_method;
}

std::vector<std::string> EnsembleStrategy::validateTargetModels(const std::vector<std::string>& target_models) {
    std::vector<std::string> valid_models;
    
    for (const auto& model_name : target_models) {
        auto model = m_registry.getModel(model_name);
        if (model && model->isHealthy()) {
            valid_models.push_back(model_name);
        } else {
            Logger::getInstance().warning("EnsembleStrategy", 
                "Model not available or unhealthy: " + model_name);
        }
    }
    
    return valid_models;
}

QualityAnalysis EnsembleStrategy::defaultQualityAnalyzer(const std::string& prompt, 
                                                        const std::string& response) {
    QualityAnalysis analysis;
    
    if (response.empty()) {
        return analysis;
    }
    
    // Overall quality score components
    double length_score = 0.5;
    double structure_score = 0.5;
    double relevance_score = 0.5;
    double coherence_score = 0.5;
    
    // Length appropriateness
    double length_ratio = static_cast<double>(response.length()) / prompt.length();
    if (length_ratio >= 0.5 && length_ratio <= 20.0) {
        length_score = 0.8;
    } else if (length_ratio >= 0.2 && length_ratio <= 50.0) {
        length_score = 0.6;
    }
    
    // Structure and formatting
    size_t line_count = std::count(response.begin(), response.end(), '\n');
    size_t sentence_count = std::count(response.begin(), response.end(), '.') +
                           std::count(response.begin(), response.end(), '!') +
                           std::count(response.begin(), response.end(), '?');
    
    if (line_count > 2 && sentence_count > 1) {
        structure_score = 0.8;
    } else if (sentence_count > 0) {
        structure_score = 0.6;
    }
    
    // Relevance (keyword overlap)
    std::regex word_regex(R"(\b\w{3,}\b)");
    std::unordered_set<std::string> prompt_words, response_words;
    
    std::sregex_iterator prompt_iter(prompt.begin(), prompt.end(), word_regex);
    std::sregex_iterator response_iter(response.begin(), response.end(), word_regex);
    std::sregex_iterator end;
    
    for (; prompt_iter != end; ++prompt_iter) {
        std::string word = prompt_iter->str();
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        prompt_words.insert(word);
    }
    
    for (; response_iter != end; ++response_iter) {
        std::string word = response_iter->str();
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        response_words.insert(word);
    }
    
    size_t overlap = 0;
    for (const auto& word : prompt_words) {
        if (response_words.count(word)) {
            overlap++;
        }
    }
    
    if (!prompt_words.empty()) {
        relevance_score = 0.3 + 0.7 * (static_cast<double>(overlap) / prompt_words.size());
    }
    
    // Coherence (basic checks)
    std::string lower_response = response;
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(), ::tolower);
    
    if (lower_response.find("error") == std::string::npos &&
        lower_response.find("sorry") == std::string::npos &&
        lower_response.find("cannot") == std::string::npos) {
        coherence_score = 0.8;
    }
    
    // Combine scores
    analysis.overall_score = 0.3 * length_score + 0.2 * structure_score + 
                            0.3 * relevance_score + 0.2 * coherence_score;
    analysis.completeness_score = length_score;
    analysis.coherence_score = coherence_score;
    analysis.relevance_score = relevance_score;
    analysis.accuracy_score = (structure_score + coherence_score) / 2.0;
    
    // Component scores
    analysis.component_scores["length"] = length_score;
    analysis.component_scores["structure"] = structure_score;
    analysis.component_scores["relevance"] = relevance_score;
    analysis.component_scores["coherence"] = coherence_score;
    
    // Positive/negative factors
    if (length_score > 0.7) analysis.positive_factors.push_back("appropriate_length");
    if (structure_score > 0.7) analysis.positive_factors.push_back("good_structure");
    if (relevance_score > 0.7) analysis.positive_factors.push_back("high_relevance");
    if (coherence_score > 0.7) analysis.positive_factors.push_back("coherent_response");
    
    if (length_score < 0.4) analysis.negative_factors.push_back("inappropriate_length");
    if (structure_score < 0.4) analysis.negative_factors.push_back("poor_structure");
    if (relevance_score < 0.4) analysis.negative_factors.push_back("low_relevance");
    if (coherence_score < 0.4) analysis.negative_factors.push_back("incoherent_response");
    
    analysis.analysis_summary = "Quality analysis complete. Overall score: " + 
                               std::to_string(analysis.overall_score);
    
    return analysis;
}

std::string EnsembleStrategy::defaultResponseCombiner(const std::vector<ModelResponse>& responses) {
    if (responses.empty()) {
        return "";
    }
    
    // Simple approach: return the highest quality response
    auto best_response = std::max_element(responses.begin(), responses.end(),
                                        [](const ModelResponse& a, const ModelResponse& b) {
                                            return a.quality_score < b.quality_score;
                                        });
    
    return best_response->response_text;
}

std::vector<std::string> EnsembleStrategy::extractDecisionFactors(
    EnsembleMethod method,
    const std::vector<ModelResponse>& responses,
    const std::string& final_response) {
    
    std::vector<std::string> factors;
    
    factors.push_back("method_" + ensembleMethodToString(method));
    factors.push_back("models_count_" + std::to_string(responses.size()));
    
    if (!responses.empty()) {
        auto best_response = std::max_element(responses.begin(), responses.end(),
                                            [](const ModelResponse& a, const ModelResponse& b) {
                                                return a.quality_score < b.quality_score;
                                            });
        
        factors.push_back("best_model_" + best_response->model_name);
        factors.push_back("best_quality_" + std::to_string(best_response->quality_score));
        
        // Check if final response matches best individual response
        if (calculateResponseSimilarity(final_response, best_response->response_text) > 0.8) {
            factors.push_back("selected_best_individual");
        } else {
            factors.push_back("ensemble_combination_used");
        }
    }
    
    return factors;
}

std::string ensembleMethodToString(EnsembleMethod method) {
    switch (method) {
        case EnsembleMethod::VOTING: return "voting";
        case EnsembleMethod::WEIGHTED_AVERAGING: return "weighted_averaging";
        case EnsembleMethod::BEST_OF_N: return "best_of_n";
        case EnsembleMethod::CONSENSUS_BUILDING: return "consensus_building";
        case EnsembleMethod::HYBRID: return "hybrid";
        default: return "unknown";
    }
}

EnsembleMethod stringToEnsembleMethod(const std::string& str) {
    if (str == "voting") return EnsembleMethod::VOTING;
    if (str == "weighted_averaging") return EnsembleMethod::WEIGHTED_AVERAGING;
    if (str == "best_of_n") return EnsembleMethod::BEST_OF_N;
    if (str == "consensus_building") return EnsembleMethod::CONSENSUS_BUILDING;
    if (str == "hybrid") return EnsembleMethod::HYBRID;
    return EnsembleMethod::BEST_OF_N; // Default
}

} // namespace Camus