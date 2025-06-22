// =================================================================
// src/Camus/MultiFileDiff.cpp
// =================================================================
// Implementation for displaying comprehensive diffs across multiple files.

#include "Camus/MultiFileDiff.hpp"
#include "dtl/dtl.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace Camus {

// Color definitions
const std::string MultiFileDiff::Colors::RESET = "\033[0m";
const std::string MultiFileDiff::Colors::RED = "\033[31m";
const std::string MultiFileDiff::Colors::GREEN = "\033[32m";
const std::string MultiFileDiff::Colors::YELLOW = "\033[33m";
const std::string MultiFileDiff::Colors::BLUE = "\033[34m";
const std::string MultiFileDiff::Colors::MAGENTA = "\033[35m";
const std::string MultiFileDiff::Colors::CYAN = "\033[36m";
const std::string MultiFileDiff::Colors::BOLD = "\033[1m";
const std::string MultiFileDiff::Colors::DIM = "\033[2m";

MultiFileDiff::MultiFileDiff(const std::string& project_root)
    : m_project_root(std::filesystem::absolute(project_root).string()),
      m_use_pager(true),
      m_confirmation_prompt("Apply changes to these files? [y/n/a/q]: "),
      m_per_file_confirmation(false) {
}

bool MultiFileDiff::showDiffsAndConfirm(const std::vector<FileModification>& modifications) {
    return showDiffsAndConfirm(modifications, m_default_options);
}

bool MultiFileDiff::showDiffsAndConfirm(const std::vector<FileModification>& modifications,
                                       const DiffDisplayOptions& options) {
    // Reset statistics
    m_last_stats = DiffStats();
    m_last_stats.total_files = modifications.size();
    
    if (modifications.empty()) {
        std::cout << "No files to modify." << std::endl;
        return false;
    }
    
    // Count new vs modified files
    for (const auto& mod : modifications) {
        if (mod.is_new_file) {
            m_last_stats.files_added++;
        } else {
            m_last_stats.files_modified++;
        }
    }
    
    std::ostringstream all_diffs;
    
    // Display header
    if (options.color_output && supportsColor()) {
        all_diffs << Colors::BOLD << Colors::BLUE;
    }
    all_diffs << "\n=== PROPOSED CHANGES ===" << std::endl;
    all_diffs << "Total files: " << m_last_stats.total_files 
              << " (+" << m_last_stats.files_added 
              << " new, ~" << m_last_stats.files_modified 
              << " modified)" << std::endl;
    if (options.color_output && supportsColor()) {
        all_diffs << Colors::RESET;
    }
    all_diffs << std::endl;
    
    // Process each file
    size_t file_index = 0;
    for (const auto& modification : modifications) {
        file_index++;
        
        // Get original content (empty for new files)
        std::string original_content;
        if (!modification.is_new_file) {
            original_content = loadOriginalContent(modification.file_path);
        }
        
        // Generate and display diff
        if (m_per_file_confirmation) {
            // Display diff for single file
            std::cout << all_diffs.str();
            all_diffs.str("");
            all_diffs.clear();
            
            displayFileDiff(modification, options, file_index, modifications.size());
            
            // Get confirmation for this file
            std::string response = getUserConfirmation(
                "Apply changes to " + modification.file_path + "? [y/n/a/q]: ");
            
            if (response == "q" || response == "quit") {
                std::cout << "Aborted by user." << std::endl;
                return false;
            } else if (response == "n" || response == "no") {
                continue; // Skip this file
            } else if (response == "a" || response == "all") {
                m_per_file_confirmation = false; // Apply all remaining
            }
        } else {
            // Accumulate all diffs
            std::ostringstream file_diff;
            
            // File header
            if (options.color_output && supportsColor()) {
                file_diff << Colors::BOLD << Colors::CYAN;
            }
            file_diff << "\n--- " << (file_index) << "/" << modifications.size() 
                     << ": " << modification.file_path;
            if (modification.is_new_file) {
                file_diff << " (NEW FILE)";
            }
            file_diff << " ---" << std::endl;
            if (options.color_output && supportsColor()) {
                file_diff << Colors::RESET;
            }
            
            // Generate unified diff
            std::string diff = generateUnifiedDiff(original_content, 
                                                  modification.new_content,
                                                  modification.file_path,
                                                  options);
            file_diff << diff;
            
            // File statistics
            if (options.show_file_stats) {
                DiffStats file_stats;
                calculateFileStats(original_content, modification.new_content, file_stats);
                
                if (options.color_output && supportsColor()) {
                    file_diff << Colors::DIM;
                }
                file_diff << "  Changes: ";
                if (file_stats.total_additions > 0) {
                    file_diff << "+" << file_stats.total_additions << " ";
                }
                if (file_stats.total_deletions > 0) {
                    file_diff << "-" << file_stats.total_deletions << " ";
                }
                file_diff << "(~" << file_stats.total_unchanged << " unchanged)";
                file_diff << "  Size: " << formatFileSize(modification.new_content.size());
                if (options.color_output && supportsColor()) {
                    file_diff << Colors::RESET;
                }
                file_diff << std::endl;
                
                // Update total stats
                m_last_stats.total_additions += file_stats.total_additions;
                m_last_stats.total_deletions += file_stats.total_deletions;
                m_last_stats.total_unchanged += file_stats.total_unchanged;
            }
            
            all_diffs << file_diff.str();
        }
    }
    
    // Display summary
    all_diffs << std::endl;
    if (options.color_output && supportsColor()) {
        all_diffs << Colors::BOLD;
    }
    all_diffs << "=== SUMMARY ===" << std::endl;
    all_diffs << "Total changes: +" << m_last_stats.total_additions 
              << " -" << m_last_stats.total_deletions << std::endl;
    if (options.color_output && supportsColor()) {
        all_diffs << Colors::RESET;
    }
    
    // Display all diffs (with pager if needed)
    std::string diff_content = all_diffs.str();
    if (m_use_pager && diff_content.length() > 2000) {
        displayWithPager(diff_content);
    } else {
        std::cout << diff_content;
    }
    
    // Get final confirmation
    std::string response = getUserConfirmation(m_confirmation_prompt);
    
    return (response == "y" || response == "yes" || response == "a" || response == "all");
}

const DiffStats& MultiFileDiff::getLastDiffStats() const {
    return m_last_stats;
}

void MultiFileDiff::setUsePager(bool use_pager) {
    m_use_pager = use_pager;
}

void MultiFileDiff::setConfirmationPrompt(const std::string& prompt) {
    m_confirmation_prompt = prompt;
}

void MultiFileDiff::setPerFileConfirmation(bool enabled) {
    m_per_file_confirmation = enabled;
}

void MultiFileDiff::displayFileDiff(const FileModification& modification,
                                   const DiffDisplayOptions& options,
                                   size_t file_index, size_t total_files) {
    // File header with progress
    if (options.color_output && supportsColor()) {
        std::cout << Colors::BOLD << Colors::CYAN;
    }
    std::cout << "\n" << generateProgressBar(file_index, total_files) << std::endl;
    std::cout << "File " << file_index << "/" << total_files 
              << ": " << modification.file_path;
    if (modification.is_new_file) {
        std::cout << " (NEW FILE)";
    }
    std::cout << std::endl;
    if (options.color_output && supportsColor()) {
        std::cout << Colors::RESET;
    }
    
    // Get original content
    std::string original_content;
    if (!modification.is_new_file) {
        original_content = loadOriginalContent(modification.file_path);
    }
    
    // Generate and display diff
    std::string diff = generateUnifiedDiff(original_content,
                                          modification.new_content,
                                          modification.file_path,
                                          options);
    std::cout << diff;
    
    // File statistics
    if (options.show_file_stats) {
        DiffStats file_stats;
        calculateFileStats(original_content, modification.new_content, file_stats);
        
        if (options.color_output && supportsColor()) {
            std::cout << Colors::DIM;
        }
        std::cout << "\nChanges: ";
        if (file_stats.total_additions > 0) {
            std::cout << "+" << file_stats.total_additions << " ";
        }
        if (file_stats.total_deletions > 0) {
            std::cout << "-" << file_stats.total_deletions << " ";
        }
        std::cout << "(~" << file_stats.total_unchanged << " unchanged)";
        std::cout << "  Size: " << formatFileSize(modification.new_content.size());
        if (options.color_output && supportsColor()) {
            std::cout << Colors::RESET;
        }
        std::cout << std::endl;
    }
}

void MultiFileDiff::displaySummary(const std::vector<FileModification>& modifications) {
    std::cout << "\n=== MODIFICATION SUMMARY ===" << std::endl;
    std::cout << "Files to modify: " << modifications.size() << std::endl;
    
    // Group by file type
    std::unordered_map<std::string, size_t> extension_counts;
    for (const auto& mod : modifications) {
        std::filesystem::path path(mod.file_path);
        std::string ext = path.extension().string();
        if (ext.empty()) ext = "(no extension)";
        extension_counts[ext]++;
    }
    
    std::cout << "\nBy file type:" << std::endl;
    for (const auto& [ext, count] : extension_counts) {
        std::cout << "  " << ext << ": " << count << " files" << std::endl;
    }
    
    std::cout << "\nTotal changes: +" << m_last_stats.total_additions 
              << " -" << m_last_stats.total_deletions << std::endl;
}

std::string MultiFileDiff::loadOriginalContent(const std::string& file_path) const {
    std::filesystem::path full_path = std::filesystem::path(m_project_root) / file_path;
    
    if (!std::filesystem::exists(full_path)) {
        return ""; // New file
    }
    
    std::ifstream file(full_path);
    if (!file.is_open()) {
        std::cerr << "[WARN] Could not read original file: " << file_path << std::endl;
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

std::string MultiFileDiff::generateUnifiedDiff(const std::string& original,
                                              const std::string& modified,
                                              const std::string& file_path,
                                              const DiffDisplayOptions& options) const {
    // Convert to lines
    auto original_lines = textToLines(original);
    auto modified_lines = textToLines(modified);
    
    // Use dtl library for diff
    using elem = std::string;
    using sequence = std::vector<elem>;
    dtl::Diff<elem, sequence> diff(original_lines, modified_lines);
    diff.compose();
    
    std::ostringstream result;
    
    // Generate unified diff format
    size_t context = options.context_lines;
    auto ses = diff.getSes().getSequence();
    
    for (size_t i = 0; i < ses.size(); ) {
        // Find next change
        while (i < ses.size() && ses[i].second.type == dtl::SES_COMMON) {
            i++;
        }
        
        if (i >= ses.size()) break;
        
        // Collect changes in this hunk
        size_t hunk_start = i;
        size_t old_start = (i >= context) ? i - context : 0;
        size_t new_start = old_start;
        
        // Find end of hunk
        size_t hunk_end = i;
        while (hunk_end < ses.size() && 
               (ses[hunk_end].second.type != dtl::SES_COMMON || 
                (hunk_end + 1 < ses.size() && hunk_end + 1 - i < context * 2))) {
            hunk_end++;
        }
        
        // Display hunk
        for (size_t j = old_start; j < std::min(hunk_end + context, ses.size()); j++) {
            const auto& ses_item = ses[j];
            char type_char = ' ';
            std::string color;
            
            if (ses_item.second.type == dtl::SES_DELETE) {
                type_char = '-';
                color = options.color_output && supportsColor() ? Colors::RED : "";
            } else if (ses_item.second.type == dtl::SES_ADD) {
                type_char = '+';
                color = options.color_output && supportsColor() ? Colors::GREEN : "";
            } else {
                color = options.color_output && supportsColor() ? Colors::DIM : "";
            }
            
            std::string line = ses_item.first;
            if (line.length() > options.max_line_length) {
                line = truncateLine(line, options.max_line_length);
            }
            
            result << color << type_char << " ";
            if (options.show_line_numbers && j < original_lines.size()) {
                result << std::setw(4) << (j + 1) << ": ";
            }
            result << line;
            if (options.color_output && supportsColor()) {
                result << Colors::RESET;
            }
            result << std::endl;
        }
        
        i = hunk_end;
    }
    
    return result.str();
}

std::string MultiFileDiff::formatDiffLine(const std::string& line, char type,
                                         size_t line_num, const DiffDisplayOptions& options) const {
    std::ostringstream formatted;
    
    // Apply color based on type
    if (options.color_output && supportsColor()) {
        switch (type) {
            case '+':
                formatted << Colors::GREEN;
                break;
            case '-':
                formatted << Colors::RED;
                break;
            default:
                formatted << Colors::DIM;
                break;
        }
    }
    
    // Add type character
    formatted << type << " ";
    
    // Add line number if requested
    if (options.show_line_numbers) {
        formatted << std::setw(4) << line_num << ": ";
    }
    
    // Add line content (truncated if needed)
    std::string content = line;
    if (content.length() > options.max_line_length) {
        content = truncateLine(content, options.max_line_length);
    }
    formatted << content;
    
    // Reset color
    if (options.color_output && supportsColor()) {
        formatted << Colors::RESET;
    }
    
    return formatted.str();
}

std::string MultiFileDiff::truncateLine(const std::string& line, size_t max_length) const {
    if (line.length() <= max_length) {
        return line;
    }
    
    return line.substr(0, max_length - 3) + "...";
}

std::string MultiFileDiff::getUserConfirmation(const std::string& prompt) const {
    std::cout << std::endl << prompt;
    std::cout.flush();
    
    std::string response;
    std::getline(std::cin, response);
    
    // Convert to lowercase
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);
    
    return response;
}

void MultiFileDiff::displayWithPager(const std::string& content) const {
    // Try to use system pager
    const char* pager = std::getenv("PAGER");
    if (!pager) {
        pager = "less -R"; // -R preserves colors
    }
    
    // Create temporary file
    std::string temp_file = std::filesystem::temp_directory_path() / "camus_diff.tmp";
    std::ofstream out(temp_file);
    out << content;
    out.close();
    
    // Display with pager
    std::string command = std::string(pager) + " " + temp_file;
    std::system(command.c_str());
    
    // Clean up
    std::filesystem::remove(temp_file);
}

bool MultiFileDiff::supportsColor() const {
#if defined(_WIN32)
    // Windows 10+ supports ANSI colors
    DWORD mode;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        return true;
    }
    return false;
#else
    // Check if output is a terminal
    return isatty(fileno(stdout));
#endif
}

void MultiFileDiff::calculateFileStats(const std::string& original,
                                      const std::string& modified,
                                      DiffStats& stats) const {
    auto original_lines = textToLines(original);
    auto modified_lines = textToLines(modified);
    
    using elem = std::string;
    using sequence = std::vector<elem>;
    dtl::Diff<elem, sequence> diff(original_lines, modified_lines);
    diff.compose();
    
    for (const auto& ses_item : diff.getSes().getSequence()) {
        if (ses_item.second.type == dtl::SES_DELETE) {
            stats.total_deletions++;
        } else if (ses_item.second.type == dtl::SES_ADD) {
            stats.total_additions++;
        } else {
            stats.total_unchanged++;
        }
    }
}

std::string MultiFileDiff::formatFileSize(size_t size) const {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit_index = 0;
    double display_size = static_cast<double>(size);
    
    while (display_size > 1024 && unit_index < 3) {
        display_size /= 1024;
        unit_index++;
    }
    
    std::ostringstream result;
    result << std::fixed << std::setprecision(1) << display_size << " " << units[unit_index];
    return result.str();
}

std::string MultiFileDiff::generateProgressBar(size_t current, size_t total, size_t width) const {
    if (total == 0) return "";
    
    float progress = static_cast<float>(current) / total;
    size_t filled = static_cast<size_t>(progress * width);
    
    std::ostringstream bar;
    bar << "[";
    for (size_t i = 0; i < width; i++) {
        if (i < filled) {
            bar << "=";
        } else if (i == filled) {
            bar << ">";
        } else {
            bar << " ";
        }
    }
    bar << "] " << std::fixed << std::setprecision(1) << (progress * 100) << "%";
    
    return bar.str();
}

std::vector<std::string> MultiFileDiff::textToLines(const std::string& text) const {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    // Handle case where text doesn't end with newline
    if (!text.empty() && text.back() != '\n') {
        lines.push_back("");
    }
    
    return lines;
}

} // namespace Camus