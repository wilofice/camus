// =================================================================
// include/Camus/IgnorePattern.hpp
// =================================================================
// Header for gitignore-compatible pattern matching functionality.

#pragma once

#include <string>
#include <vector>
#include <regex>

namespace Camus {

/**
 * @brief Gitignore-compatible pattern matching utility
 * 
 * Supports the full range of gitignore pattern syntax including:
 * - Wildcards: *, **, ?
 * - Negation: !pattern
 * - Directory-only patterns: pattern/
 * - Anchored patterns: /pattern
 * - Comment lines: # comment
 */
class IgnorePattern {
public:
    /**
     * @brief Construct a pattern matcher from a gitignore-style pattern
     * @param pattern The pattern string
     */
    explicit IgnorePattern(const std::string& pattern);

    /**
     * @brief Check if a path matches this pattern
     * @param path Relative path from project root
     * @param is_directory True if the path is a directory
     * @return true if path matches the pattern
     */
    bool matches(const std::string& path, bool is_directory = false) const;

    /**
     * @brief Check if this is a negation pattern (starts with !)
     * @return true if this pattern negates matches
     */
    bool isNegation() const { return m_is_negation; }

    /**
     * @brief Check if this pattern only matches directories
     * @return true if pattern ends with /
     */
    bool isDirectoryOnly() const { return m_directory_only; }

    /**
     * @brief Get the original pattern string
     * @return The pattern as provided to constructor
     */
    const std::string& getPattern() const { return m_original_pattern; }

    /**
     * @brief Check if pattern is empty or comment
     * @return true if pattern should be ignored
     */
    bool isEmpty() const { return m_is_empty; }

private:
    std::string m_original_pattern;
    std::string m_processed_pattern;
    bool m_is_negation;
    bool m_directory_only;
    bool m_is_anchored;
    bool m_is_empty;
    std::regex m_regex;

    /**
     * @brief Process the raw pattern into internal representation
     * @param pattern Raw pattern string
     */
    void processPattern(const std::string& pattern);

    /**
     * @brief Convert gitignore glob pattern to regex
     * @param glob_pattern Glob pattern string
     * @return Equivalent regex pattern
     */
    std::string globToRegex(const std::string& glob_pattern) const;

    /**
     * @brief Escape special regex characters except glob wildcards
     * @param str String to escape
     * @return Escaped string
     */
    std::string escapeRegexExceptGlob(const std::string& str) const;
};

/**
 * @brief Collection of ignore patterns with efficient matching
 */
class IgnorePatternSet {
public:
    /**
     * @brief Add a pattern to the set
     * @param pattern Pattern string
     */
    void addPattern(const std::string& pattern);

    /**
     * @brief Load patterns from a file (e.g., .camusignore)
     * @param file_path Path to ignore file
     * @return Number of patterns loaded
     */
    size_t loadFromFile(const std::string& file_path);

    /**
     * @brief Check if a path should be ignored
     * @param path Relative path from project root
     * @param is_directory True if path is a directory
     * @return true if path should be ignored
     */
    bool shouldIgnore(const std::string& path, bool is_directory = false) const;

    /**
     * @brief Get number of patterns in the set
     * @return Pattern count
     */
    size_t size() const { return m_patterns.size(); }

    /**
     * @brief Clear all patterns
     */
    void clear() { m_patterns.clear(); }

private:
    std::vector<IgnorePattern> m_patterns;
};

} // namespace Camus