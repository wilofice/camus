// =================================================================
// include/Camus/ResponseParser.hpp
// =================================================================
// Header for parsing structured LLM responses into file modifications.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace Camus {

/**
 * @brief Represents a single file modification from LLM response
 */
struct FileModification {
    std::string file_path;        ///< Relative path to the file
    std::string new_content;      ///< Complete new file content
    bool is_new_file;            ///< True if this creates a new file
    size_t estimated_tokens;     ///< Estimated token count of new content
    
    FileModification() : is_new_file(false), estimated_tokens(0) {}
    
    FileModification(const std::string& path, const std::string& content, bool new_file = false)
        : file_path(path), new_content(content), is_new_file(new_file), estimated_tokens(0) {}
};

/**
 * @brief Statistics about the parsing operation
 */
struct ParseStats {
    size_t total_files_found;      ///< Number of file markers found
    size_t valid_files_parsed;     ///< Number of successfully parsed files
    size_t new_files_created;      ///< Number of new files to be created
    size_t existing_files_modified; ///< Number of existing files to be modified
    size_t parsing_errors;         ///< Number of parsing errors encountered
    std::vector<std::string> error_messages; ///< Detailed error messages
    
    ParseStats() : total_files_found(0), valid_files_parsed(0), new_files_created(0), 
                   existing_files_modified(0), parsing_errors(0) {}
};

/**
 * @brief Parses structured LLM responses into actionable file modifications
 * 
 * The ResponseParser handles the complex task of extracting file modifications
 * from LLM responses that follow the expected format with file markers.
 */
class ResponseParser {
public:
    /**
     * @brief Construct a new ResponseParser
     * @param project_root Root directory of the project for path validation
     */
    explicit ResponseParser(const std::string& project_root = ".");

    /**
     * @brief Parse LLM response into file modifications
     * @param llm_response Raw response from LLM
     * @return Vector of file modifications
     */
    std::vector<FileModification> parseResponse(const std::string& llm_response);

    /**
     * @brief Get statistics from the last parse operation
     * @return Parse statistics
     */
    const ParseStats& getLastParseStats() const;

    /**
     * @brief Set whether to allow creation of new files
     * @param allow True to allow new file creation
     */
    void setAllowNewFiles(bool allow);

    /**
     * @brief Set maximum allowed file size for modifications
     * @param max_size Maximum file size in bytes
     */
    void setMaxFileSize(size_t max_size);

    /**
     * @brief Add allowed file extensions for new files
     * @param extensions Set of allowed extensions (e.g., {".cpp", ".hpp"})
     */
    void setAllowedExtensions(const std::unordered_set<std::string>& extensions);

    /**
     * @brief Validate that all file paths are safe and within project
     * @param strict_validation Enable strict path validation
     */
    void setStrictValidation(bool strict_validation);

    /**
     * @brief Validate entire response before parsing individual files
     * @param llm_response Raw LLM response
     * @return True if response format appears valid
     */
    bool validateResponseFormat(const std::string& llm_response) const;

    /**
     * @brief Perform comprehensive validation of parsed modifications
     * @param modifications Vector of file modifications to validate
     * @return True if all modifications pass validation
     */
    bool validateModifications(const std::vector<FileModification>& modifications) const;

private:
    std::string m_project_root;
    ParseStats m_last_stats;
    bool m_allow_new_files;
    size_t m_max_file_size;
    std::unordered_set<std::string> m_allowed_extensions;
    bool m_strict_validation;

    /**
     * @brief Extract file markers and content from response
     * @param response LLM response text
     * @return Map of file paths to content
     */
    std::unordered_map<std::string, std::string> extractFileBlocks(const std::string& response);

    /**
     * @brief Validate a file path for safety and correctness
     * @param file_path Relative file path
     * @return True if path is valid
     */
    bool isValidFilePath(const std::string& file_path) const;

    /**
     * @brief Normalize file path (remove ./, resolve relative paths)
     * @param file_path Input file path
     * @return Normalized path
     */
    std::string normalizeFilePath(const std::string& file_path) const;

    /**
     * @brief Check if file already exists in project
     * @param file_path Relative file path
     * @return True if file exists
     */
    bool fileExists(const std::string& file_path) const;

    /**
     * @brief Get file extension from path
     * @param file_path File path
     * @return Extension including dot (e.g., ".cpp")
     */
    std::string getFileExtension(const std::string& file_path) const;

    /**
     * @brief Validate file content for basic sanity checks
     * @param content File content
     * @param file_path File path for context
     * @return True if content appears valid
     */
    bool isValidFileContent(const std::string& content, const std::string& file_path) const;

    /**
     * @brief Perform basic syntax validation for known file types
     * @param content File content
     * @param extension File extension
     * @return True if content has basic syntax validity
     */
    bool validateSyntax(const std::string& content, const std::string& extension) const;

    /**
     * @brief Clean up file content (trim whitespace, normalize line endings)
     * @param content Raw file content
     * @return Cleaned content
     */
    std::string cleanFileContent(const std::string& content) const;

    /**
     * @brief Estimate token count for content
     * @param content Text content
     * @return Estimated token count
     */
    size_t estimateTokens(const std::string& content) const;

    /**
     * @brief Add error to statistics
     * @param error_message Descriptive error message
     */
    void addError(const std::string& error_message);

    /**
     * @brief Initialize default allowed extensions
     */
    void initializeDefaultExtensions();

    /**
     * @brief Check for potential security risks in file content
     * @param content File content to check
     * @param file_path File path for context
     * @return True if content appears safe
     */
    bool checkContentSecurity(const std::string& content, const std::string& file_path) const;

    /**
     * @brief Validate file size and complexity limits
     * @param modification File modification to validate
     * @return True if within acceptable limits
     */
    bool validateFileLimits(const FileModification& modification) const;
};

} // namespace Camus