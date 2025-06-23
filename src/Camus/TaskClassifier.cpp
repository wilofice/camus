// =================================================================
// src/Camus/TaskClassifier.cpp
// =================================================================
// Implementation for intelligent task classification system.

#include "Camus/TaskClassifier.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <fstream>
#include <filesystem>

namespace Camus {

TaskClassifier::TaskClassifier(const ClassificationConfig& config) 
    : m_config(config) {
    initializeDefaultRules();
}

ClassificationResult TaskClassifier::classify(const std::string& prompt, const std::string& context) {
    auto start_time = std::chrono::steady_clock::now();
    
    ClassificationResult result;
    
    if (prompt.empty()) {
        result.primary_type = TaskType::UNKNOWN;
        result.confidence = 0.0;
        return result;
    }
    
    // Perform different types of analysis
    std::unordered_map<TaskType, double> keyword_scores;
    std::unordered_map<TaskType, double> pattern_scores;
    std::unordered_map<TaskType, double> context_scores;
    
    if (m_config.enable_keyword_analysis) {
        keyword_scores = analyzeKeywords(prompt);
    }
    
    if (m_config.enable_pattern_matching) {
        pattern_scores = analyzePatterns(prompt);
    }
    
    if (m_config.enable_context_analysis && !context.empty()) {
        context_scores = analyzeContext(prompt, context);
    }
    
    // Combine analysis results
    result = combineAnalysis(keyword_scores, pattern_scores, context_scores);
    
    // Calculate timing
    auto end_time = std::chrono::steady_clock::now();
    result.classification_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // Update statistics
    m_classification_counts[result.primary_type]++;
    
    return result;
}

ClassificationResult TaskClassifier::classifyWithContext(const std::string& prompt,
                                                        const std::vector<std::string>& file_paths,
                                                        const std::vector<std::string>& file_extensions) {
    auto start_time = std::chrono::steady_clock::now();
    
    ClassificationResult result;
    
    if (prompt.empty()) {
        result.primary_type = TaskType::UNKNOWN;
        result.confidence = 0.0;
        return result;
    }
    
    // Perform analysis with enhanced context
    std::unordered_map<TaskType, double> keyword_scores;
    std::unordered_map<TaskType, double> pattern_scores;
    std::unordered_map<TaskType, double> context_scores;
    
    if (m_config.enable_keyword_analysis) {
        keyword_scores = analyzeKeywords(prompt);
    }
    
    if (m_config.enable_pattern_matching) {
        pattern_scores = analyzePatterns(prompt);
    }
    
    if (m_config.enable_context_analysis) {
        context_scores = analyzeContext(prompt, "", file_paths, file_extensions);
    }
    
    // Combine analysis results
    result = combineAnalysis(keyword_scores, pattern_scores, context_scores);
    
    // Calculate timing
    auto end_time = std::chrono::steady_clock::now();
    result.classification_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    // Update statistics
    m_classification_counts[result.primary_type]++;
    
    return result;
}

void TaskClassifier::updateConfig(const ClassificationConfig& config) {
    m_config = config;
}

std::string TaskClassifier::getTaskTypeName(TaskType type) {
    switch (type) {
        case TaskType::CODE_GENERATION: return "CODE_GENERATION";
        case TaskType::CODE_ANALYSIS: return "CODE_ANALYSIS";
        case TaskType::REFACTORING: return "REFACTORING";
        case TaskType::BUG_FIXING: return "BUG_FIXING";
        case TaskType::DOCUMENTATION: return "DOCUMENTATION";
        case TaskType::SECURITY_REVIEW: return "SECURITY_REVIEW";
        case TaskType::SIMPLE_QUERY: return "SIMPLE_QUERY";
        case TaskType::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

TaskType TaskClassifier::getTaskTypeFromName(const std::string& name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
    
    if (normalized == "CODE_GENERATION") return TaskType::CODE_GENERATION;
    if (normalized == "CODE_ANALYSIS") return TaskType::CODE_ANALYSIS;
    if (normalized == "REFACTORING") return TaskType::REFACTORING;
    if (normalized == "BUG_FIXING") return TaskType::BUG_FIXING;
    if (normalized == "DOCUMENTATION") return TaskType::DOCUMENTATION;
    if (normalized == "SECURITY_REVIEW") return TaskType::SECURITY_REVIEW;
    if (normalized == "SIMPLE_QUERY") return TaskType::SIMPLE_QUERY;
    
    return TaskType::UNKNOWN;
}

std::vector<TaskType> TaskClassifier::getAllTaskTypes() {
    return {
        TaskType::CODE_GENERATION,
        TaskType::CODE_ANALYSIS,
        TaskType::REFACTORING,
        TaskType::BUG_FIXING,
        TaskType::DOCUMENTATION,
        TaskType::SECURITY_REVIEW,
        TaskType::SIMPLE_QUERY,
        TaskType::UNKNOWN
    };
}

std::unordered_map<TaskType, size_t> TaskClassifier::getClassificationStats() const {
    return m_classification_counts;
}

void TaskClassifier::resetStats() {
    m_classification_counts.clear();
}

void TaskClassifier::initializeDefaultRules() {
    // Initialize keywords for each task type
    m_keywords[TaskType::CODE_GENERATION] = {
        "create", "generate", "write", "implement", "build", "make", "develop",
        "add function", "new class", "create method", "implement feature",
        "write code", "generate code", "build component", "develop module"
    };
    
    m_keywords[TaskType::CODE_ANALYSIS] = {
        "analyze", "explain", "understand", "review", "examine", "inspect",
        "what does", "how does", "why does", "explain this", "analyze code",
        "review this", "understand this", "examine function", "inspect class"
    };
    
    m_keywords[TaskType::REFACTORING] = {
        "refactor", "restructure", "reorganize", "improve", "optimize", "clean up",
        "rename", "extract", "move", "split", "merge", "simplify",
        "refactor code", "improve structure", "optimize performance", "clean code"
    };
    
    m_keywords[TaskType::BUG_FIXING] = {
        "fix", "bug", "error", "issue", "problem", "broken", "crash", "fail",
        "debug", "solve", "resolve", "correct", "repair",
        "fix bug", "solve issue", "debug problem", "repair error"
    };
    
    m_keywords[TaskType::DOCUMENTATION] = {
        "document", "comment", "docs", "documentation", "describe", "annotate",
        "add comments", "write docs", "document function", "add documentation",
        "comment code", "explain function", "describe method", "annotate class"
    };
    
    m_keywords[TaskType::SECURITY_REVIEW] = {
        "security", "secure", "vulnerability", "exploit", "sanitize", "validate",
        "injection", "xss", "csrf", "authentication", "authorization", "encrypt",
        "security review", "check security", "validate input", "sanitize data"
    };
    
    m_keywords[TaskType::SIMPLE_QUERY] = {
        "what", "how", "why", "when", "where", "which", "is", "are", "can", "should",
        "quick question", "simple change", "small modification", "minor update"
    };
    
    // Initialize regex patterns for each task type
    m_patterns[TaskType::CODE_GENERATION] = {
        std::regex(R"(\b(create|write|implement|generate|build|make)\s+(a\s+)?(function|class|method|component|module|feature))", std::regex_constants::icase),
        std::regex(R"(\b(add|create)\s+(new\s+)?\w+)", std::regex_constants::icase),
        std::regex(R"(\bimplement\s+\w+)", std::regex_constants::icase)
    };
    
    m_patterns[TaskType::CODE_ANALYSIS] = {
        std::regex(R"(\b(analyze|explain|understand|review|examine)\s+(this\s+)?(code|function|class|method))", std::regex_constants::icase),
        std::regex(R"(\bwhat\s+(does|is)\s+this\s+\w+)", std::regex_constants::icase),
        std::regex(R"(\bhow\s+(does|works?)\s+this)", std::regex_constants::icase)
    };
    
    m_patterns[TaskType::REFACTORING] = {
        std::regex(R"(\b(refactor|restructure|reorganize|improve|optimize|clean\s+up))", std::regex_constants::icase),
        std::regex(R"(\b(rename|extract|move|split|merge)\s+\w+)", std::regex_constants::icase),
        std::regex(R"(\bimprove\s+(structure|performance|readability))", std::regex_constants::icase)
    };
    
    m_patterns[TaskType::BUG_FIXING] = {
        std::regex(R"(\b(fix|debug|solve|resolve|repair)\s+(this\s+)?(bug|error|issue|problem))", std::regex_constants::icase),
        std::regex(R"(\b(broken|crash|fail|not\s+working))", std::regex_constants::icase),
        std::regex(R"(\berror\s+(in|with)\s+\w+)", std::regex_constants::icase)
    };
    
    m_patterns[TaskType::DOCUMENTATION] = {
        std::regex(R"(\b(add|write|create)\s+(comments?|docs?|documentation))", std::regex_constants::icase),
        std::regex(R"(\b(document|comment|annotate)\s+(this\s+)?(function|class|method|code))", std::regex_constants::icase),
        std::regex(R"(\bexplain\s+what\s+this\s+\w+\s+does)", std::regex_constants::icase)
    };
    
    m_patterns[TaskType::SECURITY_REVIEW] = {
        std::regex(R"(\b(security|secure|vulnerability|exploit)\s+(review|check|analysis))", std::regex_constants::icase),
        std::regex(R"(\b(sanitize|validate|encrypt)\s+\w+)", std::regex_constants::icase),
        std::regex(R"(\b(sql\s+injection|xss|csrf|authentication|authorization))", std::regex_constants::icase)
    };
    
    m_patterns[TaskType::SIMPLE_QUERY] = {
        std::regex(R"(^\s*(what|how|why|when|where|which|is|are|can|should)\s+)", std::regex_constants::icase),
        std::regex(R"(\b(quick|simple|small|minor)\s+(question|change|modification|update))", std::regex_constants::icase)
    };
}

std::unordered_map<TaskType, double> TaskClassifier::analyzeKeywords(const std::string& prompt) {
    std::unordered_map<TaskType, double> scores;
    std::string normalized_prompt = normalizeText(prompt);
    auto keywords = extractKeywords(normalized_prompt);
    
    for (const auto& [task_type, task_keywords] : m_keywords) {
        double score = 0.0;
        int matches = 0;
        
        for (const auto& keyword : task_keywords) {
            std::string normalized_keyword = normalizeText(keyword);
            
            // Check for exact keyword matches
            if (normalized_prompt.find(normalized_keyword) != std::string::npos) {
                score += 1.0;
                matches++;
            }
            
            // Check for partial matches in extracted keywords
            for (const auto& extracted_keyword : keywords) {
                if (extracted_keyword.find(normalized_keyword) != std::string::npos ||
                    normalized_keyword.find(extracted_keyword) != std::string::npos) {
                    score += 0.5;
                }
            }
        }
        
        // Normalize score by number of potential matches
        if (!task_keywords.empty()) {
            scores[task_type] = score / task_keywords.size();
        }
    }
    
    return scores;
}

std::unordered_map<TaskType, double> TaskClassifier::analyzePatterns(const std::string& prompt) {
    std::unordered_map<TaskType, double> scores;
    
    for (const auto& [task_type, patterns] : m_patterns) {
        double score = 0.0;
        
        for (const auto& pattern : patterns) {
            std::smatch match;
            if (std::regex_search(prompt, match, pattern)) {
                score += 1.0;
            }
        }
        
        // Normalize score by number of patterns
        if (!patterns.empty()) {
            scores[task_type] = score / patterns.size();
        }
    }
    
    return scores;
}

std::unordered_map<TaskType, double> TaskClassifier::analyzeContext(const std::string& prompt,
                                                                   const std::string& context,
                                                                   const std::vector<std::string>& file_paths,
                                                                   const std::vector<std::string>& file_extensions) {
    std::unordered_map<TaskType, double> scores;
    
    // Analyze file extensions for context clues
    if (!file_extensions.empty()) {
        bool has_test_files = false;
        bool has_config_files = false;
        bool has_doc_files = false;
        
        for (const auto& ext : file_extensions) {
            if (ext == ".test.cpp" || ext == ".test.js" || ext == "_test.py" || ext.find("test") != std::string::npos) {
                has_test_files = true;
            }
            if (ext == ".json" || ext == ".yml" || ext == ".yaml" || ext == ".xml" || ext == ".config") {
                has_config_files = true;
            }
            if (ext == ".md" || ext == ".txt" || ext == ".rst" || ext == ".doc") {
                has_doc_files = true;
            }
        }
        
        if (has_test_files) {
            scores[TaskType::BUG_FIXING] += 0.3;
            scores[TaskType::CODE_ANALYSIS] += 0.2;
        }
        if (has_config_files) {
            scores[TaskType::REFACTORING] += 0.2;
        }
        if (has_doc_files) {
            scores[TaskType::DOCUMENTATION] += 0.5;
        }
    }
    
    // Analyze file paths for context clues
    for (const auto& path : file_paths) {
        std::string lower_path = normalizeText(path);
        
        if (lower_path.find("test") != std::string::npos || 
            lower_path.find("spec") != std::string::npos) {
            scores[TaskType::BUG_FIXING] += 0.2;
        }
        if (lower_path.find("doc") != std::string::npos || 
            lower_path.find("readme") != std::string::npos) {
            scores[TaskType::DOCUMENTATION] += 0.3;
        }
        if (lower_path.find("security") != std::string::npos || 
            lower_path.find("auth") != std::string::npos) {
            scores[TaskType::SECURITY_REVIEW] += 0.3;
        }
    }
    
    // Analyze additional context content
    if (!context.empty()) {
        std::string normalized_context = normalizeText(context);
        
        // Look for code patterns that suggest specific task types
        if (normalized_context.find("todo") != std::string::npos ||
            normalized_context.find("fixme") != std::string::npos) {
            scores[TaskType::BUG_FIXING] += 0.3;
        }
        
        if (normalized_context.find("class ") != std::string::npos ||
            normalized_context.find("function ") != std::string::npos) {
            scores[TaskType::CODE_ANALYSIS] += 0.2;
        }
    }
    
    return scores;
}

ClassificationResult TaskClassifier::combineAnalysis(const std::unordered_map<TaskType, double>& keyword_scores,
                                                    const std::unordered_map<TaskType, double>& pattern_scores,
                                                    const std::unordered_map<TaskType, double>& context_scores) {
    ClassificationResult result;
    std::unordered_map<TaskType, double> combined_scores;
    
    // Combine scores with weights
    double keyword_weight = 0.4;
    double pattern_weight = 0.4;
    double context_weight = 0.2;
    
    // Get all possible task types
    auto all_types = getAllTaskTypes();
    
    for (TaskType type : all_types) {
        double score = 0.0;
        
        // Add keyword score
        auto keyword_it = keyword_scores.find(type);
        if (keyword_it != keyword_scores.end()) {
            score += keyword_it->second * keyword_weight;
        }
        
        // Add pattern score
        auto pattern_it = pattern_scores.find(type);
        if (pattern_it != pattern_scores.end()) {
            score += pattern_it->second * pattern_weight;
        }
        
        // Add context score
        auto context_it = context_scores.find(type);
        if (context_it != context_scores.end()) {
            score += context_it->second * context_weight;
        }
        
        // Apply type weights from configuration
        auto weight_it = m_config.type_weights.find(type);
        if (weight_it != m_config.type_weights.end()) {
            score *= weight_it->second;
        }
        
        combined_scores[type] = score;
    }
    
    // Find the task type with the highest score
    auto max_element = std::max_element(combined_scores.begin(), combined_scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    if (max_element != combined_scores.end() && max_element->second > 0.0) {
        result.primary_type = max_element->first;
        result.confidence = calculateConfidence(combined_scores, max_element->second);
        result.alternatives = getAlternatives(combined_scores, result.primary_type);
        
        // Store keyword scores for debugging
        result.keyword_scores["keyword_score"] = keyword_scores.count(result.primary_type) ? 
                                                keyword_scores.at(result.primary_type) : 0.0;
        result.keyword_scores["pattern_score"] = pattern_scores.count(result.primary_type) ? 
                                               pattern_scores.at(result.primary_type) : 0.0;
        result.keyword_scores["context_score"] = context_scores.count(result.primary_type) ? 
                                               context_scores.at(result.primary_type) : 0.0;
        result.keyword_scores["combined_score"] = max_element->second;
    } else {
        result.primary_type = TaskType::UNKNOWN;
        result.confidence = 0.0;
    }
    
    // Check if confidence meets threshold
    if (result.confidence < m_config.confidence_threshold) {
        result.alternatives.insert(result.alternatives.begin(), result.primary_type);
        result.primary_type = TaskType::UNKNOWN;
        result.confidence = 0.0;
    }
    
    return result;
}

std::string TaskClassifier::normalizeText(const std::string& text) {
    std::string normalized = text;
    
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Remove extra whitespace
    normalized = std::regex_replace(normalized, std::regex(R"(\s+)"), " ");
    
    // Trim whitespace
    normalized.erase(0, normalized.find_first_not_of(" \t\n\r"));
    normalized.erase(normalized.find_last_not_of(" \t\n\r") + 1);
    
    return normalized;
}

std::vector<std::string> TaskClassifier::extractKeywords(const std::string& text) {
    std::vector<std::string> keywords;
    std::istringstream iss(text);
    std::string word;
    
    while (iss >> word) {
        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
        
        // Skip very short words and common stop words
        if (word.length() >= 3 && 
            word != "the" && word != "and" && word != "for" && 
            word != "are" && word != "but" && word != "not" &&
            word != "you" && word != "all" && word != "can" &&
            word != "had" && word != "her" && word != "was" &&
            word != "one" && word != "our" && word != "out" &&
            word != "day" && word != "get" && word != "has" &&
            word != "him" && word != "his" && word != "how" &&
            word != "its" && word != "may" && word != "new" &&
            word != "now" && word != "old" && word != "see" &&
            word != "two" && word != "way" && word != "who" &&
            word != "boy" && word != "did" && word != "has" &&
            word != "let" && word != "put" && word != "say" &&
            word != "she" && word != "too" && word != "use") {
            keywords.push_back(word);
        }
    }
    
    return keywords;
}

double TaskClassifier::calculateConfidence(const std::unordered_map<TaskType, double>& scores, double top_score) {
    if (scores.size() <= 1) {
        return top_score;
    }
    
    // Find second highest score
    double second_score = 0.0;
    for (const auto& [type, score] : scores) {
        if (score < top_score && score > second_score) {
            second_score = score;
        }
    }
    
    // Calculate confidence based on separation from second best
    if (second_score > 0.0) {
        double separation = (top_score - second_score) / top_score;
        return std::min(1.0, top_score * (0.5 + 0.5 * separation));
    }
    
    return top_score;
}

std::vector<TaskType> TaskClassifier::getAlternatives(const std::unordered_map<TaskType, double>& scores,
                                                     TaskType primary_type,
                                                     size_t max_alternatives) {
    std::vector<std::pair<TaskType, double>> sorted_scores;
    
    for (const auto& [type, score] : scores) {
        if (type != primary_type && score > 0.0) {
            sorted_scores.emplace_back(type, score);
        }
    }
    
    // Sort by score descending
    std::sort(sorted_scores.begin(), sorted_scores.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<TaskType> alternatives;
    for (size_t i = 0; i < std::min(max_alternatives, sorted_scores.size()); ++i) {
        alternatives.push_back(sorted_scores[i].first);
    }
    
    return alternatives;
}

std::string taskTypeToString(TaskType task_type) {
    switch (task_type) {
        case TaskType::CODE_GENERATION:
            return "CODE_GENERATION";
        case TaskType::CODE_ANALYSIS:
            return "CODE_ANALYSIS";
        case TaskType::REFACTORING:
            return "REFACTORING";
        case TaskType::BUG_FIXING:
            return "BUG_FIXING";
        case TaskType::DOCUMENTATION:
            return "DOCUMENTATION";
        case TaskType::SECURITY_REVIEW:
            return "SECURITY_REVIEW";
        case TaskType::SIMPLE_QUERY:
            return "SIMPLE_QUERY";
        case TaskType::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

TaskType stringToTaskType(const std::string& str) {
    if (str == "CODE_GENERATION") return TaskType::CODE_GENERATION;
    if (str == "CODE_ANALYSIS") return TaskType::CODE_ANALYSIS;
    if (str == "REFACTORING") return TaskType::REFACTORING;
    if (str == "BUG_FIXING") return TaskType::BUG_FIXING;
    if (str == "DOCUMENTATION") return TaskType::DOCUMENTATION;
    if (str == "SECURITY_REVIEW") return TaskType::SECURITY_REVIEW;
    if (str == "SIMPLE_QUERY") return TaskType::SIMPLE_QUERY;
    return TaskType::UNKNOWN;
}

} // namespace Camus