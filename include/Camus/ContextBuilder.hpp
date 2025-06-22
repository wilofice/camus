// =================================================================
// include/Camus/ContextBuilder.hpp
// =================================================================
// Header for building large context prompts with smart truncation.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace Camus {

/**
 * @brief File information for context building
 */
struct FileInfo {
    std::string relative_path;
    std::string content;
    size_t file_size;
    std::filesystem::file_time_type last_modified;
    int priority_score;
    
    FileInfo(const std::string& path, const std::string& file_content) 
        : relative_path(path), content(file_content), file_size(file_content.size()), priority_score(0) {}
};

/**
 * @brief Builds context prompts for LLM with intelligent content management
 * 
 * The ContextBuilder efficiently constructs large context prompts while managing
 * token limits through smart truncation, file prioritization, and content optimization.
 */
class ContextBuilder {
public:
    /**
     * @brief Construct a new ContextBuilder
     * @param max_tokens Maximum tokens to include in context (default: 128k)
     */
    explicit ContextBuilder(size_t max_tokens = 128000);

    /**
     * @brief Build context prompt from file paths and user request
     * @param file_paths Vector of relative file paths to include
     * @param user_request The user's modification request
     * @param root_path Root directory path for reading files
     * @return Complete context prompt string
     */
    std::string buildContext(const std::vector<std::string>& file_paths, 
                           const std::string& user_request,
                           const std::string& root_path = ".");

    /**
     * @brief Set maximum token limit
     * @param max_tokens New token limit
     */
    void setMaxTokens(size_t max_tokens);

    /**
     * @brief Set reserved tokens for system prompt and response
     * @param reserved_tokens Tokens to reserve (default: 8000)
     */
    void setReservedTokens(size_t reserved_tokens);

    /**
     * @brief Add file type priority weight
     * @param extension File extension (e.g., ".hpp")
     * @param weight Priority weight (higher = more important)
     */
    void setFileTypePriority(const std::string& extension, int weight);

    /**
     * @brief Enable git-based prioritization (recently changed files get higher priority)
     * @param enabled Whether to use git information for prioritization
     */
    void setGitPrioritization(bool enabled);

    /**
     * @brief Add keywords that boost file relevance
     * @param keywords Vector of keywords to search for in file content
     */
    void setRelevanceKeywords(const std::vector<std::string>& keywords);

    /**
     * @brief Get statistics from last context build
     * @return Map of statistics (files_included, tokens_used, files_truncated, etc.)
     */
    std::unordered_map<std::string, size_t> getLastBuildStats() const;

private:
    size_t m_max_tokens;
    size_t m_reserved_tokens;
    std::unordered_map<std::string, int> m_file_type_priorities;
    std::unordered_map<std::string, size_t> m_last_stats;
    bool m_git_prioritization_enabled;
    std::vector<std::string> m_relevance_keywords;

    /**
     * @brief Estimate token count for text (rough approximation: 4 chars ≈ 1 token)
     * @param text Text to estimate
     * @return Estimated token count
     */
    size_t estimateTokens(const std::string& text) const;

    /**
     * @brief Load and prepare file information
     * @param file_paths List of file paths
     * @param root_path Root directory
     * @return Vector of FileInfo structures
     */
    std::vector<FileInfo> loadFileInfo(const std::vector<std::string>& file_paths, 
                                      const std::string& root_path) const;

    /**
     * @brief Calculate priority score for a file
     * @param file_info File information
     * @return Priority score (higher = more important)
     */
    int calculateFilePriority(const FileInfo& file_info) const;

    /**
     * @brief Prioritize files based on various factors
     * @param files Vector of file information
     * @return Sorted vector (highest priority first)
     */
    std::vector<FileInfo> prioritizeFiles(std::vector<FileInfo> files) const;

    /**
     * @brief Format file content for inclusion in prompt
     * @param file_info File information
     * @param truncated Whether content was truncated
     * @return Formatted file content with markers
     */
    std::string formatFileContent(const FileInfo& file_info, bool truncated = false) const;

    /**
     * @brief Truncate content while preserving structure
     * @param content Original content
     * @param max_tokens Maximum tokens for this content
     * @return Truncated content with preservation markers
     */
    std::string intelligentTruncate(const std::string& content, size_t max_tokens) const;

    /**
     * @brief Build the system prompt for amodify command
     * @return System prompt string
     */
    std::string buildSystemPrompt() const;

    /**
     * @brief Build user prompt with request and context
     * @param user_request User's modification request
     * @param formatted_files All formatted file contents
     * @return User prompt string
     */
    std::string buildUserPrompt(const std::string& user_request, 
                               const std::string& formatted_files) const;

    /**
     * @brief Initialize default file type priorities
     */
    void initializeDefaultPriorities();

    /**
     * @brief Get file extension from path
     * @param file_path File path
     * @return Extension including dot (e.g., ".cpp")
     */
    std::string getFileExtension(const std::string& file_path) const;

    /**
     * @brief Check if content should be truncated based on file type
     * @param extension File extension
     * @return true if file type should be truncated when needed
     */
    bool shouldTruncateFileType(const std::string& extension) const;

    /**
     * @brief Get git-based priority for a file (based on recent changes)
     * @param file_path Relative path to file
     * @return Priority bonus from git history
     */
    int getGitPriority(const std::string& file_path) const;

    /**
     * @brief Calculate relevance score based on keywords
     * @param content File content
     * @return Relevance score
     */
    int calculateRelevanceScore(const std::string& content) const;
};

} // namespace Camus