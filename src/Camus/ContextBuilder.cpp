// =================================================================
// src/Camus/ContextBuilder.cpp
// =================================================================
// Implementation for building large context prompts with smart truncation.

#include "Camus/ContextBuilder.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <unordered_set>

namespace Camus {

ContextBuilder::ContextBuilder(size_t max_tokens) 
    : m_max_tokens(max_tokens), m_reserved_tokens(8000), m_git_prioritization_enabled(true) {
    initializeDefaultPriorities();
}

std::string ContextBuilder::buildContext(const std::vector<std::string>& file_paths, 
                                       const std::string& user_request,
                                       const std::string& root_path) {
    
    // Reset statistics
    m_last_stats.clear();
    m_last_stats["files_total"] = file_paths.size();
    m_last_stats["files_included"] = 0;
    m_last_stats["files_truncated"] = 0;
    m_last_stats["tokens_used"] = 0;
    
    std::cout << "[INFO] Building context from " << file_paths.size() << " files..." << std::endl;
    
    // Load and prioritize files
    auto file_infos = loadFileInfo(file_paths, root_path);
    auto prioritized_files = prioritizeFiles(std::move(file_infos));
    
    // Build system prompt
    std::string system_prompt = buildSystemPrompt();
    size_t system_tokens = estimateTokens(system_prompt);
    
    // Reserve tokens for user request and response
    size_t available_tokens = m_max_tokens - m_reserved_tokens - system_tokens;
    size_t user_request_tokens = estimateTokens(user_request);
    available_tokens = available_tokens > user_request_tokens ? 
                      available_tokens - user_request_tokens : 0;
    
    std::cout << "[INFO] Available tokens for file content: " << available_tokens << std::endl;
    
    // Build file content within token limits
    std::ostringstream files_content;
    size_t used_tokens = 0;
    
    for (const auto& file_info : prioritized_files) {
        size_t file_tokens = estimateTokens(file_info.content);
        bool needs_truncation = false;
        std::string content_to_use = file_info.content;
        
        // Check if we need to truncate or skip this file
        if (used_tokens + file_tokens > available_tokens) {
            size_t remaining_tokens = available_tokens - used_tokens;
            
            if (remaining_tokens < 100) {
                // Not enough tokens left, skip remaining files
                std::cout << "[WARN] Token limit reached, skipping " 
                         << (prioritized_files.size() - m_last_stats["files_included"]) 
                         << " remaining files" << std::endl;
                break;
            }
            
            // Truncate the file content
            content_to_use = intelligentTruncate(file_info.content, remaining_tokens - 50); // Reserve 50 tokens for formatting
            file_tokens = estimateTokens(content_to_use);
            needs_truncation = true;
            m_last_stats["files_truncated"]++;
        }
        
        // Create FileInfo with potentially truncated content
        FileInfo display_info = file_info;
        display_info.content = content_to_use;
        
        files_content << formatFileContent(display_info, needs_truncation);
        used_tokens += file_tokens + 20; // Add some padding for formatting
        m_last_stats["files_included"]++;
        
        if (needs_truncation) {
            break; // Stop after truncating a file to avoid further complications
        }
    }
    
    // Build complete prompt
    std::string formatted_files = files_content.str();
    std::string user_prompt = buildUserPrompt(user_request, formatted_files);
    std::string complete_prompt = system_prompt + user_prompt;
    
    m_last_stats["tokens_used"] = estimateTokens(complete_prompt);
    
    std::cout << "[INFO] Context built: " << m_last_stats["files_included"] 
              << " files, ~" << m_last_stats["tokens_used"] << " tokens" << std::endl;
    
    return complete_prompt;
}

void ContextBuilder::setMaxTokens(size_t max_tokens) {
    m_max_tokens = max_tokens;
}

void ContextBuilder::setReservedTokens(size_t reserved_tokens) {
    m_reserved_tokens = reserved_tokens;
}

void ContextBuilder::setFileTypePriority(const std::string& extension, int weight) {
    m_file_type_priorities[extension] = weight;
}

void ContextBuilder::setGitPrioritization(bool enabled) {
    m_git_prioritization_enabled = enabled;
}

void ContextBuilder::setRelevanceKeywords(const std::vector<std::string>& keywords) {
    m_relevance_keywords = keywords;
}

std::unordered_map<std::string, size_t> ContextBuilder::getLastBuildStats() const {
    return m_last_stats;
}

size_t ContextBuilder::estimateTokens(const std::string& text) const {
    // Rough approximation: 4 characters ≈ 1 token
    // This accounts for spaces, punctuation, and typical English text patterns
    return (text.length() + 3) / 4;
}

std::vector<FileInfo> ContextBuilder::loadFileInfo(const std::vector<std::string>& file_paths, 
                                                  const std::string& root_path) const {
    std::vector<FileInfo> file_infos;
    file_infos.reserve(file_paths.size());
    
    for (const auto& relative_path : file_paths) {
        std::string full_path = root_path + "/" + relative_path;
        
        try {
            std::ifstream file(full_path);
            if (!file.is_open()) {
                std::cerr << "[WARN] Could not open file: " << relative_path << std::endl;
                continue;
            }
            
            std::ostringstream content_stream;
            content_stream << file.rdbuf();
            std::string content = content_stream.str();
            
            FileInfo info(relative_path, content);
            
            // Get file modification time
            std::filesystem::path fs_path(full_path);
            if (std::filesystem::exists(fs_path)) {
                info.last_modified = std::filesystem::last_write_time(fs_path);
            }
            
            // Calculate priority score
            info.priority_score = calculateFilePriority(info);
            
            file_infos.push_back(std::move(info));
            
        } catch (const std::exception& e) {
            std::cerr << "[WARN] Error loading file " << relative_path << ": " << e.what() << std::endl;
        }
    }
    
    return file_infos;
}

int ContextBuilder::calculateFilePriority(const FileInfo& file_info) const {
    int priority = 0;
    
    // File type priority
    std::string extension = getFileExtension(file_info.relative_path);
    auto type_priority_it = m_file_type_priorities.find(extension);
    if (type_priority_it != m_file_type_priorities.end()) {
        priority += type_priority_it->second;
    }
    
    // Size priority (smaller files get slight boost)
    if (file_info.file_size < 1000) {
        priority += 10; // Small files
    } else if (file_info.file_size < 5000) {
        priority += 5;  // Medium files
    }
    // Large files get no size bonus
    
    // Path depth priority (shallower paths get slight boost)
    size_t depth = std::count(file_info.relative_path.begin(), file_info.relative_path.end(), '/');
    priority += std::max(0, 5 - static_cast<int>(depth));
    
    // Git-based priority (recently changed files)
    if (m_git_prioritization_enabled) {
        priority += getGitPriority(file_info.relative_path);
    }
    
    // Relevance-based priority (keyword matching)
    if (!m_relevance_keywords.empty()) {
        priority += calculateRelevanceScore(file_info.content);
    }
    
    return priority;
}

std::vector<FileInfo> ContextBuilder::prioritizeFiles(std::vector<FileInfo> files) const {
    // Sort by priority score (highest first), then by modification time (newest first)
    std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
        if (a.priority_score != b.priority_score) {
            return a.priority_score > b.priority_score;
        }
        return a.last_modified > b.last_modified;
    });
    
    return files;
}

std::string ContextBuilder::formatFileContent(const FileInfo& file_info, bool truncated) const {
    std::ostringstream formatted;
    
    formatted << "\n--- FILE: " << file_info.relative_path << " ---\n";
    
    if (truncated) {
        formatted << "// [TRUNCATED - Content exceeds token limit]\n";
    }
    
    formatted << file_info.content;
    
    // Ensure content ends with newline
    if (!file_info.content.empty() && file_info.content.back() != '\n') {
        formatted << '\n';
    }
    
    return formatted.str();
}

std::string ContextBuilder::intelligentTruncate(const std::string& content, size_t max_tokens) const {
    size_t content_tokens = estimateTokens(content);
    if (content_tokens <= max_tokens) {
        return content;
    }
    
    // Calculate approximate character limit
    size_t char_limit = max_tokens * 3; // Conservative estimate
    
    if (content.length() <= char_limit) {
        return content;
    }
    
    // Try to find a good truncation point (end of line, function, etc.)
    std::string truncated = content.substr(0, char_limit);
    
    // Find the last complete line
    size_t last_newline = truncated.find_last_of('\n');
    if (last_newline != std::string::npos && last_newline > char_limit / 2) {
        truncated = truncated.substr(0, last_newline + 1);
    }
    
    truncated += "\n\n// [CONTENT TRUNCATED - File continues beyond token limit]\n";
    
    return truncated;
}

std::string ContextBuilder::buildSystemPrompt() const {
    return R"(<|begin_of_text|><|start_header_id|>system<|end_header_id|>

You are an expert software engineer assistant capable of analyzing entire codebases and making comprehensive modifications across multiple files. Your task is to understand the user's high-level request and determine which files need to be modified and how.

CRITICAL INSTRUCTIONS:
1. Analyze the entire project context provided below
2. Understand the codebase architecture, patterns, and conventions
3. Determine which files need to be modified to fulfill the user's request
4. Respond with ONLY the modified files in the specified format
5. Include the COMPLETE content for each file you want to modify
6. Do NOT include explanations, reasoning, or conversational text
7. Follow existing code style, patterns, and architectural decisions
8. Ensure all modifications work together cohesively

RESPONSE FORMAT:
For each file you want to modify, use this exact format:

--- FILE: path/to/file.ext ---
[complete file content here]

IMPORTANT NOTES:
- Only include files that actually need changes
- Include the full, complete content of each modified file
- Maintain all existing functionality unless explicitly requested to change it
- Follow the project's existing conventions and patterns
- Ensure your changes are syntactically correct and compile properly

<|eot_id|><|start_header_id|>user<|end_header_id|>

)";
}

std::string ContextBuilder::buildUserPrompt(const std::string& user_request, 
                                          const std::string& formatted_files) const {
    std::ostringstream prompt;
    
    prompt << "Implement the following request: " << user_request << "\n\n";
    prompt << "Here is the full project context:\n";
    prompt << formatted_files;
    prompt << "\n--- END OF PROJECT CONTEXT ---\n\n";
    prompt << "<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";
    
    return prompt.str();
}

void ContextBuilder::initializeDefaultPriorities() {
    // Header files - highest priority for understanding interfaces
    m_file_type_priorities[".hpp"] = 100;
    m_file_type_priorities[".h"] = 95;
    m_file_type_priorities[".hxx"] = 90;
    
    // Main source files - high priority
    m_file_type_priorities[".cpp"] = 80;
    m_file_type_priorities[".c"] = 75;
    m_file_type_priorities[".cc"] = 75;
    m_file_type_priorities[".cxx"] = 75;
    
    // Configuration files - medium-high priority
    m_file_type_priorities[".cmake"] = 70;
    m_file_type_priorities[".yml"] = 65;
    m_file_type_priorities[".yaml"] = 65;
    m_file_type_priorities[".json"] = 60;
    m_file_type_priorities[".toml"] = 60;
    
    // Build and project files
    m_file_type_priorities[".make"] = 55;
    m_file_type_priorities[".mk"] = 55;
    
    // Scripts - medium priority
    m_file_type_priorities[".sh"] = 50;
    m_file_type_priorities[".bash"] = 50;
    m_file_type_priorities[".py"] = 45;
    
    // Web/Frontend files - medium priority
    m_file_type_priorities[".js"] = 40;
    m_file_type_priorities[".ts"] = 42;
    m_file_type_priorities[".jsx"] = 38;
    m_file_type_priorities[".tsx"] = 40;
    
    // Documentation - lower priority
    m_file_type_priorities[".md"] = 30;
    m_file_type_priorities[".txt"] = 25;
    m_file_type_priorities[".rst"] = 25;
    
    // Config/ignore files - lowest priority
    m_file_type_priorities[".gitignore"] = 10;
    m_file_type_priorities[".gitattributes"] = 10;
}

std::string ContextBuilder::getFileExtension(const std::string& file_path) const {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "";
    }
    return file_path.substr(dot_pos);
}

bool ContextBuilder::shouldTruncateFileType(const std::string& extension) const {
    // Don't truncate critical files like headers and small config files
    static const std::unordered_set<std::string> no_truncate = {
        ".hpp", ".h", ".hxx", ".cmake", ".yml", ".yaml", ".json"
    };
    
    return no_truncate.find(extension) == no_truncate.end();
}

int ContextBuilder::getGitPriority(const std::string& file_path) const {
    // Simple implementation - could be enhanced with actual git command execution
    // For now, return a basic priority based on common change patterns
    
    // Files in src/ or include/ that were recently modified get higher priority
    if (file_path.find("src/") == 0 || file_path.find("include/") == 0) {
        return 15; // Recent core changes
    }
    
    // Configuration files that might have been recently changed
    if (file_path.find("CMakeLists.txt") != std::string::npos ||
        file_path.find(".yml") != std::string::npos ||
        file_path.find(".yaml") != std::string::npos) {
        return 10; // Recent config changes
    }
    
    return 0; // No git-based priority
}

int ContextBuilder::calculateRelevanceScore(const std::string& content) const {
    int score = 0;
    
    // Convert content to lowercase for case-insensitive matching
    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), 
                   lower_content.begin(), ::tolower);
    
    // Score based on keyword frequency
    for (const auto& keyword : m_relevance_keywords) {
        std::string lower_keyword = keyword;
        std::transform(lower_keyword.begin(), lower_keyword.end(), 
                       lower_keyword.begin(), ::tolower);
        
        size_t pos = 0;
        int keyword_count = 0;
        while ((pos = lower_content.find(lower_keyword, pos)) != std::string::npos) {
            keyword_count++;
            pos += lower_keyword.length();
            
            // Limit counting to avoid excessive scores for very repetitive content
            if (keyword_count >= 10) break;
        }
        
        // Each keyword occurrence adds to relevance score
        score += keyword_count * 5;
    }
    
    return std::min(score, 50); // Cap relevance score at 50
}

} // namespace Camus