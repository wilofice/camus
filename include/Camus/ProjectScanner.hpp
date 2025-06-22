// =================================================================
// include/Camus/ProjectScanner.hpp
// =================================================================
// Header for project file discovery and filtering functionality.

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include "IgnorePattern.hpp"

namespace Camus {

/**
 * @brief Scans project directories to discover relevant source files
 * 
 * The ProjectScanner recursively walks directory structures, applying
 * intelligent filtering based on file extensions, ignore patterns,
 * and file characteristics to identify files suitable for LLM context.
 */
class ProjectScanner {
public:
    /**
     * @brief Construct a new ProjectScanner
     * @param root_path The root directory to scan from
     */
    explicit ProjectScanner(const std::string& root_path);

    /**
     * @brief Scan the project directory and return relevant files
     * @return Vector of relative file paths that should be included in context
     */
    std::vector<std::string> scanFiles();

    /**
     * @brief Add a custom ignore pattern
     * @param pattern Gitignore-style pattern to ignore
     */
    void addIgnorePattern(const std::string& pattern);

    /**
     * @brief Add a file extension to include in scanning
     * @param extension File extension including the dot (e.g., ".cpp")
     */
    void addIncludeExtension(const std::string& extension);

    /**
     * @brief Set maximum file size to consider (in bytes)
     * @param max_size Maximum file size in bytes (default: 1MB)
     */
    void setMaxFileSize(size_t max_size);

private:
    std::string m_root_path;
    IgnorePatternSet m_ignore_patterns;
    std::unordered_set<std::string> m_include_extensions;
    size_t m_max_file_size;

    /**
     * @brief Check if a file should be ignored based on patterns
     * @param relative_path Relative path from project root
     * @return true if file should be ignored
     */
    bool shouldIgnoreFile(const std::string& relative_path) const;

    /**
     * @brief Check if a file appears to be a text file (not binary)
     * @param file_path Absolute path to the file
     * @return true if file appears to be text
     */
    bool isTextFile(const std::string& file_path) const;

    /**
     * @brief Load ignore patterns from .camusignore file
     */
    void loadCamusIgnore();

    /**
     * @brief Initialize default ignore patterns and extensions
     */
    void initializeDefaults();


    /**
     * @brief Convert filesystem path to relative string path
     * @param abs_path Absolute filesystem path
     * @return Relative path string from project root
     */
    std::string getRelativePath(const std::filesystem::path& abs_path) const;
};

} // namespace Camus