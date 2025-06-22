// =================================================================
// include/Camus/MultiFileDiff.hpp
// =================================================================
// Header for displaying comprehensive diffs across multiple files.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "ResponseParser.hpp"

namespace Camus {

/**
 * @brief Statistics for multi-file diff operations
 */
struct DiffStats {
    size_t total_files;          ///< Total number of files to be modified
    size_t files_added;          ///< Number of new files to be created
    size_t files_modified;       ///< Number of existing files to be modified
    size_t total_additions;      ///< Total lines added across all files
    size_t total_deletions;      ///< Total lines deleted across all files
    size_t total_unchanged;      ///< Total unchanged lines across all files
    
    DiffStats() : total_files(0), files_added(0), files_modified(0),
                  total_additions(0), total_deletions(0), total_unchanged(0) {}
};

/**
 * @brief Options for diff display
 */
struct DiffDisplayOptions {
    size_t context_lines;        ///< Number of context lines to show (default: 3)
    bool show_line_numbers;      ///< Show line numbers in diff (default: true)
    bool color_output;           ///< Enable colored output (default: true)
    bool show_file_stats;        ///< Show per-file statistics (default: true)
    bool compact_mode;           ///< Compact mode for large diffs (default: false)
    size_t max_line_length;      ///< Maximum line length before truncation (default: 120)
    
    DiffDisplayOptions() : context_lines(3), show_line_numbers(true), 
                          color_output(true), show_file_stats(true),
                          compact_mode(false), max_line_length(120) {}
};

/**
 * @brief Handles displaying and confirming diffs across multiple files
 * 
 * The MultiFileDiff class provides a comprehensive interface for reviewing
 * changes across multiple files with advanced display options and user interaction.
 */
class MultiFileDiff {
public:
    /**
     * @brief Construct a new MultiFileDiff
     * @param project_root Root directory of the project
     */
    explicit MultiFileDiff(const std::string& project_root = ".");

    /**
     * @brief Show diffs and get user confirmation for modifications
     * @param modifications Vector of file modifications to display
     * @return True if user confirms all changes
     */
    bool showDiffsAndConfirm(const std::vector<FileModification>& modifications);

    /**
     * @brief Show diffs with custom options and get confirmation
     * @param modifications Vector of file modifications
     * @param options Display options
     * @return True if user confirms changes
     */
    bool showDiffsAndConfirm(const std::vector<FileModification>& modifications,
                            const DiffDisplayOptions& options);

    /**
     * @brief Get statistics from last diff operation
     * @return Diff statistics
     */
    const DiffStats& getLastDiffStats() const;

    /**
     * @brief Set whether to use pager for large diffs
     * @param use_pager Enable pager (less/more)
     */
    void setUsePager(bool use_pager);

    /**
     * @brief Set custom confirmation prompt
     * @param prompt Custom prompt text
     */
    void setConfirmationPrompt(const std::string& prompt);

    /**
     * @brief Enable per-file confirmation mode
     * @param enabled True to confirm each file individually
     */
    void setPerFileConfirmation(bool enabled);

private:
    std::string m_project_root;
    DiffStats m_last_stats;
    DiffDisplayOptions m_default_options;
    bool m_use_pager;
    std::string m_confirmation_prompt;
    bool m_per_file_confirmation;

    // Color codes for terminal output
    struct Colors {
        static const std::string RESET;
        static const std::string RED;
        static const std::string GREEN;
        static const std::string YELLOW;
        static const std::string BLUE;
        static const std::string MAGENTA;
        static const std::string CYAN;
        static const std::string BOLD;
        static const std::string DIM;
    };

    /**
     * @brief Display diff for a single file
     * @param modification File modification to display
     * @param options Display options
     * @param file_index Current file index (for progress display)
     * @param total_files Total number of files
     */
    void displayFileDiff(const FileModification& modification, 
                        const DiffDisplayOptions& options,
                        size_t file_index, size_t total_files);

    /**
     * @brief Display overall summary of all modifications
     * @param modifications All file modifications
     */
    void displaySummary(const std::vector<FileModification>& modifications);

    /**
     * @brief Load original content of a file
     * @param file_path Relative file path
     * @return Original file content (empty if new file)
     */
    std::string loadOriginalContent(const std::string& file_path) const;

    /**
     * @brief Generate unified diff between two contents
     * @param original Original file content
     * @param modified Modified file content
     * @param file_path File path for context
     * @param options Display options
     * @return Formatted diff string
     */
    std::string generateUnifiedDiff(const std::string& original,
                                   const std::string& modified,
                                   const std::string& file_path,
                                   const DiffDisplayOptions& options) const;

    /**
     * @brief Format a diff line with colors
     * @param line Diff line content
     * @param type Line type (add/remove/context)
     * @param line_num Line number
     * @param options Display options
     * @return Formatted line
     */
    std::string formatDiffLine(const std::string& line, char type,
                              size_t line_num, const DiffDisplayOptions& options) const;

    /**
     * @brief Truncate long lines for display
     * @param line Line to truncate
     * @param max_length Maximum length
     * @return Truncated line with indicator
     */
    std::string truncateLine(const std::string& line, size_t max_length) const;

    /**
     * @brief Get user confirmation for changes
     * @param prompt Confirmation prompt
     * @return User response
     */
    std::string getUserConfirmation(const std::string& prompt) const;

    /**
     * @brief Display diff using system pager
     * @param content Diff content to display
     */
    void displayWithPager(const std::string& content) const;

    /**
     * @brief Check if output supports color
     * @return True if terminal supports color
     */
    bool supportsColor() const;

    /**
     * @brief Calculate statistics for a single file diff
     * @param original Original content
     * @param modified Modified content
     * @param stats Stats to update
     */
    void calculateFileStats(const std::string& original,
                           const std::string& modified,
                           DiffStats& stats) const;

    /**
     * @brief Format file size for display
     * @param size Size in bytes
     * @return Human-readable size string
     */
    std::string formatFileSize(size_t size) const;

    /**
     * @brief Generate progress bar for file processing
     * @param current Current file index
     * @param total Total files
     * @param width Progress bar width
     * @return Progress bar string
     */
    std::string generateProgressBar(size_t current, size_t total, size_t width = 40) const;

    /**
     * @brief Convert string to lines for diff processing
     * @param text Text to convert
     * @return Vector of lines
     */
    std::vector<std::string> textToLines(const std::string& text) const;
};

} // namespace Camus