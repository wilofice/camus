// =================================================================
// src/Camus/IgnorePattern.cpp
// =================================================================
// Implementation for gitignore-compatible pattern matching.

#include "Camus/IgnorePattern.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>

namespace Camus {

IgnorePattern::IgnorePattern(const std::string& pattern) 
    : m_original_pattern(pattern),
      m_is_negation(false),
      m_directory_only(false),
      m_is_anchored(false),
      m_is_empty(false)
{
    processPattern(pattern);
}

bool IgnorePattern::matches(const std::string& path, bool is_directory) const {
    if (m_is_empty) {
        return false;
    }
    
    // Directory-only patterns only match directories
    if (m_directory_only && !is_directory) {
        return false;
    }
    
    try {
        // For anchored patterns, match from start
        if (m_is_anchored) {
            return std::regex_match(path, m_regex);
        } else {
            // For unanchored patterns, match any substring or path component
            return std::regex_search(path, m_regex) || 
                   std::regex_match(path, m_regex);
        }
    } catch (const std::regex_error& e) {
        std::cerr << "[WARN] Regex error in pattern '" << m_original_pattern 
                  << "': " << e.what() << std::endl;
        return false;
    }
}

void IgnorePattern::processPattern(const std::string& pattern) {
    std::string working_pattern = pattern;
    
    // Skip empty lines and comments
    if (working_pattern.empty() || working_pattern[0] == '#') {
        m_is_empty = true;
        return;
    }
    
    // Trim whitespace
    working_pattern.erase(0, working_pattern.find_first_not_of(" \t"));
    working_pattern.erase(working_pattern.find_last_not_of(" \t") + 1);
    
    if (working_pattern.empty()) {
        m_is_empty = true;
        return;
    }
    
    // Handle negation patterns
    if (working_pattern[0] == '!') {
        m_is_negation = true;
        working_pattern = working_pattern.substr(1);
    }
    
    // Handle directory-only patterns
    if (!working_pattern.empty() && working_pattern.back() == '/') {
        m_directory_only = true;
        working_pattern.pop_back();
    }
    
    // Handle anchored patterns (starting with /)
    if (!working_pattern.empty() && working_pattern[0] == '/') {
        m_is_anchored = true;
        working_pattern = working_pattern.substr(1);
    }
    
    if (working_pattern.empty()) {
        m_is_empty = true;
        return;
    }
    
    m_processed_pattern = working_pattern;
    
    // Convert to regex
    std::string regex_pattern = globToRegex(working_pattern);
    
    try {
        m_regex = std::regex(regex_pattern, std::regex_constants::ECMAScript);
    } catch (const std::regex_error& e) {
        std::cerr << "[WARN] Failed to compile regex for pattern '" 
                  << pattern << "': " << e.what() << std::endl;
        m_is_empty = true;
    }
}

std::string IgnorePattern::globToRegex(const std::string& glob_pattern) const {
    std::string regex_pattern;
    bool in_brackets = false;
    
    for (size_t i = 0; i < glob_pattern.length(); ++i) {
        char c = glob_pattern[i];
        
        switch (c) {
            case '*':
                if (i + 1 < glob_pattern.length() && glob_pattern[i + 1] == '*') {
                    // ** matches any number of path components
                    if (i + 2 < glob_pattern.length() && glob_pattern[i + 2] == '/') {
                        regex_pattern += "(?:.*/)?";
                        i += 2; // Skip the next * and /
                    } else if (i + 2 == glob_pattern.length()) {
                        regex_pattern += ".*";
                        i += 1; // Skip the next *
                    } else {
                        regex_pattern += "[^/]*";
                    }
                } else {
                    // * matches anything except /
                    regex_pattern += "[^/]*";
                }
                break;
                
            case '?':
                // ? matches any single character except /
                regex_pattern += "[^/]";
                break;
                
            case '[':
                in_brackets = true;
                regex_pattern += '[';
                break;
                
            case ']':
                in_brackets = false;
                regex_pattern += ']';
                break;
                
            case '\\':
                // Escape the next character
                if (i + 1 < glob_pattern.length()) {
                    regex_pattern += '\\';
                    regex_pattern += glob_pattern[++i];
                } else {
                    regex_pattern += "\\\\";
                }
                break;
                
            default:
                // Escape special regex characters
                if (!in_brackets && (c == '.' || c == '^' || c == '$' || c == '+' || 
                    c == '{' || c == '}' || c == '|' || c == '(' || c == ')')) {
                    regex_pattern += '\\';
                }
                regex_pattern += c;
                break;
        }
    }
    
    // For unanchored patterns, allow matching path components
    if (!m_is_anchored) {
        if (regex_pattern.find('/') != std::string::npos) {
            // Pattern contains /, match as path component
            regex_pattern = "(^|.*/)(" + regex_pattern + ")(/.*|$)";
        } else {
            // Pattern doesn't contain /, match as filename
            regex_pattern = "(^|.*/)(" + regex_pattern + ")$";
        }
    } else {
        // Anchored pattern, match from start
        regex_pattern = "^" + regex_pattern + "(/.*|$)";
    }
    
    return regex_pattern;
}

std::string IgnorePattern::escapeRegexExceptGlob(const std::string& str) const {
    std::string result;
    for (char c : str) {
        if (c == '.' || c == '^' || c == '$' || c == '+' || 
            c == '{' || c == '}' || c == '|' || c == '(' || c == ')' ||
            c == '\\') {
            result += '\\';
        }
        result += c;
    }
    return result;
}

// IgnorePatternSet implementation

void IgnorePatternSet::addPattern(const std::string& pattern) {
    IgnorePattern ignore_pattern(pattern);
    if (!ignore_pattern.isEmpty()) {
        m_patterns.push_back(std::move(ignore_pattern));
    }
}

size_t IgnorePatternSet::loadFromFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return 0;
    }
    
    size_t patterns_loaded = 0;
    std::string line;
    while (std::getline(file, line)) {
        IgnorePattern pattern(line);
        if (!pattern.isEmpty()) {
            m_patterns.push_back(std::move(pattern));
            patterns_loaded++;
        }
    }
    
    return patterns_loaded;
}

bool IgnorePatternSet::shouldIgnore(const std::string& path, bool is_directory) const {
    bool should_ignore = false;
    
    // Process patterns in order - later patterns can override earlier ones
    for (const auto& pattern : m_patterns) {
        if (pattern.matches(path, is_directory)) {
            if (pattern.isNegation()) {
                should_ignore = false; // Negation pattern - don't ignore
            } else {
                should_ignore = true;  // Regular pattern - ignore
            }
        }
    }
    
    return should_ignore;
}

} // namespace Camus