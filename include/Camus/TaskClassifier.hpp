// =================================================================
// include/Camus/TaskClassifier.hpp
// =================================================================
// Header for intelligent task classification system.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <regex>
#include <chrono>

namespace Camus {

/**
 * @brief Task classification categories for intelligent model routing
 */
enum class TaskType {
    CODE_GENERATION,    ///< Creating new code from descriptions
    CODE_ANALYSIS,      ///< Understanding and explaining existing code
    REFACTORING,        ///< Restructuring existing code
    BUG_FIXING,         ///< Identifying and fixing issues
    DOCUMENTATION,      ///< Adding comments and documentation
    SECURITY_REVIEW,    ///< Security analysis and validation
    SIMPLE_QUERY,       ///< Quick questions and simple modifications
    UNKNOWN             ///< Unclassified or ambiguous tasks
};

/**
 * @brief Classification result with confidence metrics
 */
struct ClassificationResult {
    TaskType primary_type;              ///< Most likely task type
    double confidence;                  ///< Confidence score (0.0 to 1.0)
    std::vector<TaskType> alternatives; ///< Alternative classifications
    std::unordered_map<std::string, double> keyword_scores; ///< Keyword analysis scores
    long classification_time_ms;        ///< Time taken for classification
    
    ClassificationResult() 
        : primary_type(TaskType::UNKNOWN), confidence(0.0), classification_time_ms(0) {}
};

/**
 * @brief Configuration for task classification rules and patterns
 */
struct ClassificationConfig {
    double confidence_threshold;        ///< Minimum confidence for classification
    bool enable_keyword_analysis;       ///< Enable keyword-based analysis
    bool enable_pattern_matching;       ///< Enable regex pattern matching
    bool enable_context_analysis;       ///< Enable context-aware analysis
    std::unordered_map<TaskType, double> type_weights; ///< Weights for different task types
    
    ClassificationConfig() 
        : confidence_threshold(0.7), enable_keyword_analysis(true),
          enable_pattern_matching(true), enable_context_analysis(true) {}
};

/**
 * @brief Intelligent task classification system for routing requests
 * 
 * Analyzes user prompts and determines the most appropriate task type
 * for intelligent model selection and pipeline routing.
 */
class TaskClassifier {
public:
    /**
     * @brief Construct a new TaskClassifier
     * @param config Classification configuration
     */
    explicit TaskClassifier(const ClassificationConfig& config = ClassificationConfig());

    /**
     * @brief Classify a user prompt to determine task type
     * @param prompt User's natural language request
     * @param context Optional context information (file contents, etc.)
     * @return Classification result with confidence metrics
     */
    ClassificationResult classify(const std::string& prompt, 
                                 const std::string& context = "");

    /**
     * @brief Classify with additional file context
     * @param prompt User's natural language request
     * @param file_paths List of files being modified
     * @param file_extensions File extensions for context
     * @return Classification result with enhanced context analysis
     */
    ClassificationResult classifyWithContext(const std::string& prompt,
                                            const std::vector<std::string>& file_paths,
                                            const std::vector<std::string>& file_extensions = {});

    /**
     * @brief Update classification configuration
     * @param config New configuration settings
     */
    void updateConfig(const ClassificationConfig& config);

    /**
     * @brief Get current classification configuration
     * @return Current configuration
     */
    const ClassificationConfig& getConfig() const { return m_config; }

    /**
     * @brief Get human-readable task type name
     * @param type Task type enum value
     * @return String representation of task type
     */
    static std::string getTaskTypeName(TaskType type);

    /**
     * @brief Get task type from string name
     * @param name String representation of task type
     * @return Task type enum value
     */
    static TaskType getTaskTypeFromName(const std::string& name);

    /**
     * @brief Get all supported task types
     * @return Vector of all task type enum values
     */
    static std::vector<TaskType> getAllTaskTypes();

    /**
     * @brief Load classification rules from configuration file
     * @param config_path Path to classification rules configuration
     * @return True if loaded successfully
     */
    bool loadRulesFromFile(const std::string& config_path);

    /**
     * @brief Get classification statistics
     * @return Map of task types to classification counts
     */
    std::unordered_map<TaskType, size_t> getClassificationStats() const;

    /**
     * @brief Reset classification statistics
     */
    void resetStats();

private:
    ClassificationConfig m_config;
    
    // Keyword patterns for each task type
    std::unordered_map<TaskType, std::vector<std::string>> m_keywords;
    std::unordered_map<TaskType, std::vector<std::regex>> m_patterns;
    
    // Classification statistics
    std::unordered_map<TaskType, size_t> m_classification_counts;
    
    /**
     * @brief Initialize default classification rules
     */
    void initializeDefaultRules();

    /**
     * @brief Perform keyword-based analysis
     * @param prompt User prompt to analyze
     * @return Map of task types to keyword scores
     */
    std::unordered_map<TaskType, double> analyzeKeywords(const std::string& prompt);

    /**
     * @brief Perform pattern-based analysis
     * @param prompt User prompt to analyze
     * @return Map of task types to pattern match scores
     */
    std::unordered_map<TaskType, double> analyzePatterns(const std::string& prompt);

    /**
     * @brief Perform context-aware analysis
     * @param prompt User prompt
     * @param context Additional context information
     * @param file_paths List of file paths (optional)
     * @param file_extensions List of file extensions (optional)
     * @return Map of task types to context scores
     */
    std::unordered_map<TaskType, double> analyzeContext(const std::string& prompt,
                                                        const std::string& context,
                                                        const std::vector<std::string>& file_paths = {},
                                                        const std::vector<std::string>& file_extensions = {});

    /**
     * @brief Combine analysis results to determine final classification
     * @param keyword_scores Keyword analysis results
     * @param pattern_scores Pattern analysis results
     * @param context_scores Context analysis results
     * @return Combined classification result
     */
    ClassificationResult combineAnalysis(const std::unordered_map<TaskType, double>& keyword_scores,
                                        const std::unordered_map<TaskType, double>& pattern_scores,
                                        const std::unordered_map<TaskType, double>& context_scores);

    /**
     * @brief Normalize text for analysis (lowercase, remove punctuation, etc.)
     * @param text Input text
     * @return Normalized text
     */
    std::string normalizeText(const std::string& text);

    /**
     * @brief Extract keywords from text
     * @param text Input text
     * @return Vector of extracted keywords
     */
    std::vector<std::string> extractKeywords(const std::string& text);

    /**
     * @brief Calculate confidence score based on analysis results
     * @param scores Combined scores for all task types
     * @param top_score Highest score among all types
     * @return Confidence score (0.0 to 1.0)
     */
    double calculateConfidence(const std::unordered_map<TaskType, double>& scores, 
                              double top_score);

    /**
     * @brief Get alternative classifications
     * @param scores Combined scores for all task types
     * @param primary_type Primary classification
     * @param max_alternatives Maximum number of alternatives to return
     * @return Vector of alternative task types
     */
    std::vector<TaskType> getAlternatives(const std::unordered_map<TaskType, double>& scores,
                                         TaskType primary_type,
                                         size_t max_alternatives = 3);
};

/**
 * @brief Convert TaskType to string representation
 * @param task_type The task type to convert
 * @return String representation of the task type
 */
std::string taskTypeToString(TaskType task_type);

/**
 * @brief Convert string to TaskType
 * @param str String representation of task type
 * @return TaskType enum value
 */
TaskType stringToTaskType(const std::string& str);

} // namespace Camus