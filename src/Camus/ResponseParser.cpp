// =================================================================
// src/Camus/ResponseParser.cpp
// =================================================================
// Implementation for parsing structured LLM responses into file modifications.

#include "Camus/ResponseParser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <cctype>

namespace Camus {

ResponseParser::ResponseParser(const std::string& project_root) 
    : m_project_root(std::filesystem::absolute(project_root).string()),
      m_allow_new_files(true),
      m_max_file_size(1024 * 1024), // 1MB default
      m_strict_validation(true) {
    initializeDefaultExtensions();
}

std::vector<FileModification> ResponseParser::parseResponse(const std::string& llm_response) {
    // Reset statistics
    m_last_stats = ParseStats();
    
    std::cout << "[INFO] Parsing LLM response for file modifications..." << std::endl;
    
    // Validate response format first
    if (!validateResponseFormat(llm_response)) {
        addError("Invalid response format - no valid file markers found");
        return {};
    }
    
    // Extract file blocks from response
    auto file_blocks = extractFileBlocks(llm_response);
    m_last_stats.total_files_found = file_blocks.size();
    
    std::vector<FileModification> modifications;
    modifications.reserve(file_blocks.size());
    
    for (const auto& [raw_path, content] : file_blocks) {
        try {
            // Normalize and validate file path
            std::string normalized_path = normalizeFilePath(raw_path);
            
            if (!isValidFilePath(normalized_path)) {
                addError("Invalid file path: " + raw_path);
                continue;
            }
            
            // Clean and validate content
            std::string cleaned_content = cleanFileContent(content);
            
            if (!isValidFileContent(cleaned_content, normalized_path)) {
                addError("Invalid content for file: " + normalized_path);
                continue;
            }
            
            // Check file size limits
            if (cleaned_content.size() > m_max_file_size) {
                addError("File too large: " + normalized_path + " (" + 
                        std::to_string(cleaned_content.size()) + " bytes)");
                continue;
            }
            
            // Determine if this is a new file
            bool is_new = !fileExists(normalized_path);
            
            if (is_new && !m_allow_new_files) {
                addError("New file creation not allowed: " + normalized_path);
                continue;
            }
            
            if (is_new) {
                // Validate extension for new files
                std::string extension = getFileExtension(normalized_path);
                if (!m_allowed_extensions.empty() && 
                    m_allowed_extensions.find(extension) == m_allowed_extensions.end()) {
                    addError("File extension not allowed: " + extension + " for " + normalized_path);
                    continue;
                }
            }
            
            // Perform basic syntax validation
            std::string extension = getFileExtension(normalized_path);
            if (!validateSyntax(cleaned_content, extension)) {
                addError("Syntax validation failed for: " + normalized_path);
                continue;
            }
            
            // Check for security risks
            if (!checkContentSecurity(cleaned_content, normalized_path)) {
                addError("Security validation failed for: " + normalized_path);
                continue;
            }
            
            // Create file modification
            FileModification modification(normalized_path, cleaned_content, is_new);
            modification.estimated_tokens = estimateTokens(cleaned_content);
            
            // Validate file limits
            if (!validateFileLimits(modification)) {
                addError("File limits validation failed for: " + normalized_path);
                continue;
            }
            
            modifications.push_back(std::move(modification));
            
            if (is_new) {
                m_last_stats.new_files_created++;
            } else {
                m_last_stats.existing_files_modified++;
            }
            
            m_last_stats.valid_files_parsed++;
            
        } catch (const std::exception& e) {
            addError("Error processing file " + raw_path + ": " + e.what());
        }
    }
    
    std::cout << "[INFO] Parsed " << m_last_stats.valid_files_parsed << " valid file modifications" << std::endl;
    
    if (m_last_stats.parsing_errors > 0) {
        std::cout << "[WARN] " << m_last_stats.parsing_errors << " parsing errors encountered" << std::endl;
    }
    
    // Final validation of all modifications
    if (!validateModifications(modifications)) {
        addError("Final validation failed for modification set");
        return {};
    }
    
    return modifications;
}

const ParseStats& ResponseParser::getLastParseStats() const {
    return m_last_stats;
}

void ResponseParser::setAllowNewFiles(bool allow) {
    m_allow_new_files = allow;
}

void ResponseParser::setMaxFileSize(size_t max_size) {
    m_max_file_size = max_size;
}

void ResponseParser::setAllowedExtensions(const std::unordered_set<std::string>& extensions) {
    m_allowed_extensions = extensions;
}

void ResponseParser::setStrictValidation(bool strict_validation) {
    m_strict_validation = strict_validation;
}

std::unordered_map<std::string, std::string> ResponseParser::extractFileBlocks(const std::string& response) {
    std::unordered_map<std::string, std::string> file_blocks;
    
    // Regex to match file markers: --- FILE: path ---
    std::regex file_marker_regex(R"(^---\s*FILE:\s*(.+?)\s*---\s*$)", std::regex_constants::multiline);
    
    std::sregex_iterator iter(response.begin(), response.end(), file_marker_regex);
    std::sregex_iterator end;
    
    while (iter != end) {
        std::smatch match = *iter;
        std::string file_path = match[1].str();
        
        // Find the start of content (after the marker line)
        size_t content_start = match.suffix().first - response.begin();
        
        // Find the next file marker or end of response
        std::sregex_iterator next_iter = iter;
        ++next_iter;
        
        size_t content_end;
        if (next_iter != end) {
            content_end = next_iter->prefix().second - response.begin();
        } else {
            content_end = response.length();
        }
        
        // Extract content between markers
        if (content_start < content_end) {
            std::string content = response.substr(content_start, content_end - content_start);
            
            // Remove any trailing file markers or separators
            size_t last_marker = content.rfind("--- FILE:");
            if (last_marker != std::string::npos) {
                content = content.substr(0, last_marker);
            }
            
            file_blocks[file_path] = content;
        }
        
        ++iter;
    }
    
    return file_blocks;
}

bool ResponseParser::isValidFilePath(const std::string& file_path) const {
    if (file_path.empty()) {
        return false;
    }
    
    // Check for path traversal attempts
    if (file_path.find("..") != std::string::npos) {
        return false;
    }
    
    // Check for absolute paths (should be relative)
    if (file_path[0] == '/' || (file_path.length() > 1 && file_path[1] == ':')) {
        return false;
    }
    
    // Check for invalid characters
    static const std::string invalid_chars = "<>:\"|?*";
    if (file_path.find_first_of(invalid_chars) != std::string::npos) {
        return false;
    }
    
    // Check path length
    if (file_path.length() > 260) { // Windows MAX_PATH limitation
        return false;
    }
    
    // Validate path components
    std::filesystem::path path(file_path);
    for (const auto& component : path) {
        std::string comp_str = component.string();
        
        // Check for reserved names (Windows)
        static const std::unordered_set<std::string> reserved_names = {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
        };
        
        std::string upper_comp = comp_str;
        std::transform(upper_comp.begin(), upper_comp.end(), upper_comp.begin(), ::toupper);
        
        if (reserved_names.find(upper_comp) != reserved_names.end()) {
            return false;
        }
        
        // Check for trailing dots or spaces
        if (!comp_str.empty() && (comp_str.back() == '.' || comp_str.back() == ' ')) {
            return false;
        }
    }
    
    return true;
}

std::string ResponseParser::normalizeFilePath(const std::string& file_path) const {
    std::filesystem::path path(file_path);
    std::filesystem::path normalized = path.lexically_normal();
    return normalized.string();
}

bool ResponseParser::fileExists(const std::string& file_path) const {
    std::filesystem::path full_path = std::filesystem::path(m_project_root) / file_path;
    return std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path);
}

std::string ResponseParser::getFileExtension(const std::string& file_path) const {
    std::filesystem::path path(file_path);
    return path.extension().string();
}

bool ResponseParser::isValidFileContent(const std::string& content, const std::string& file_path) const {
    // Check for minimum content (not empty)
    if (content.empty()) {
        return false;
    }
    
    // Check for null bytes (binary content)
    if (content.find('\0') != std::string::npos) {
        return false;
    }
    
    // Check for reasonable text content ratio
    size_t printable_chars = 0;
    for (char c : content) {
        if (std::isprint(static_cast<unsigned char>(c)) || std::isspace(static_cast<unsigned char>(c))) {
            printable_chars++;
        }
    }
    
    double printable_ratio = static_cast<double>(printable_chars) / content.length();
    if (printable_ratio < 0.95) {
        return false; // Too much non-printable content
    }
    
    return true;
}

bool ResponseParser::validateSyntax(const std::string& content, const std::string& extension) const {
    if (!m_strict_validation) {
        return true; // Skip syntax validation if not strict
    }
    
    // Basic syntax validation for common file types
    if (extension == ".cpp" || extension == ".hpp" || extension == ".c" || extension == ".h") {
        // Check for basic C/C++ syntax elements
        
        // Count braces
        int brace_count = 0;
        for (char c : content) {
            if (c == '{') brace_count++;
            else if (c == '}') brace_count--;
        }
        
        if (brace_count != 0) {
            return false; // Unmatched braces
        }
        
        // Check for basic includes in headers
        if (extension == ".hpp" || extension == ".h") {
            if (content.find("#pragma once") == std::string::npos && 
                content.find("#ifndef") == std::string::npos) {
                // Headers should have include guards (warning, not error)
                std::cout << "[WARN] Header file missing include guards" << std::endl;
            }
        }
    }
    
    if (extension == ".json") {
        // Basic JSON validation - check balanced braces and brackets
        int brace_count = 0, bracket_count = 0;
        bool in_string = false;
        bool escaped = false;
        
        for (char c : content) {
            if (escaped) {
                escaped = false;
                continue;
            }
            
            if (c == '\\') {
                escaped = true;
                continue;
            }
            
            if (c == '"' && !escaped) {
                in_string = !in_string;
                continue;
            }
            
            if (!in_string) {
                if (c == '{') brace_count++;
                else if (c == '}') brace_count--;
                else if (c == '[') bracket_count++;
                else if (c == ']') bracket_count--;
            }
        }
        
        return brace_count == 0 && bracket_count == 0;
    }
    
    return true; // Default to valid for unknown file types
}

std::string ResponseParser::cleanFileContent(const std::string& content) const {
    std::string cleaned = content;
    
    // Remove leading/trailing whitespace from the entire content
    cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r"));
    cleaned.erase(cleaned.find_last_not_of(" \t\n\r") + 1);
    
    // Normalize line endings to LF
    cleaned = std::regex_replace(cleaned, std::regex("\r\n"), "\n");
    cleaned = std::regex_replace(cleaned, std::regex("\r"), "\n");
    
    // Ensure file ends with single newline
    if (!cleaned.empty() && cleaned.back() != '\n') {
        cleaned += '\n';
    }
    
    return cleaned;
}

size_t ResponseParser::estimateTokens(const std::string& content) const {
    // Same estimation as ContextBuilder: 4 chars ≈ 1 token
    return (content.length() + 3) / 4;
}

void ResponseParser::addError(const std::string& error_message) {
    m_last_stats.parsing_errors++;
    m_last_stats.error_messages.push_back(error_message);
    std::cerr << "[ERROR] " << error_message << std::endl;
}

void ResponseParser::initializeDefaultExtensions() {
    m_allowed_extensions = {
        // C/C++ files
        ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hxx",
        
        // Python files
        ".py", ".pyx", ".pyi",
        
        // JavaScript/TypeScript
        ".js", ".jsx", ".ts", ".tsx", ".mjs",
        
        // Other languages
        ".java", ".scala", ".kt", ".rs", ".go", ".rb", ".php",
        
        // Configuration files
        ".yml", ".yaml", ".json", ".toml", ".ini", ".cfg",
        ".xml", ".cmake", ".make", ".mk",
        
        // Documentation
        ".md", ".txt", ".rst",
        
        // Web files
        ".html", ".css", ".scss", ".sass",
        
        // Scripts
        ".sh", ".bash", ".zsh", ".fish",
        
        // Database
        ".sql",
        
        // Docker
        ".dockerfile",
        
        // Git files
        ".gitignore", ".gitattributes"
    };
}

bool ResponseParser::validateResponseFormat(const std::string& llm_response) const {
    // Check for minimum response length
    if (llm_response.length() < 20) {
        return false;
    }
    
    // Check for file markers
    std::regex file_marker_regex(R"(---\s*FILE:\s*.+?\s*---)");
    std::sregex_iterator iter(llm_response.begin(), llm_response.end(), file_marker_regex);
    std::sregex_iterator end;
    
    // Must have at least one file marker
    if (iter == end) {
        return false;
    }
    
    // Check that response doesn't contain obvious error patterns
    static const std::vector<std::string> error_patterns = {
        "I cannot", "I can't", "I'm sorry", "I apologize",
        "Error:", "ERROR:", "Exception:", "Failed to"
    };
    
    std::string lower_response = llm_response;
    std::transform(lower_response.begin(), lower_response.end(), 
                   lower_response.begin(), ::tolower);
    
    for (const auto& pattern : error_patterns) {
        std::string lower_pattern = pattern;
        std::transform(lower_pattern.begin(), lower_pattern.end(), 
                       lower_pattern.begin(), ::tolower);
        if (lower_response.find(lower_pattern) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

bool ResponseParser::validateModifications(const std::vector<FileModification>& modifications) const {
    if (modifications.empty()) {
        return false;
    }
    
    // Check for duplicate file paths
    std::unordered_set<std::string> seen_paths;
    for (const auto& mod : modifications) {
        if (seen_paths.find(mod.file_path) != seen_paths.end()) {
            return false; // Duplicate file path
        }
        seen_paths.insert(mod.file_path);
    }
    
    // Check total modification size
    size_t total_size = 0;
    for (const auto& mod : modifications) {
        total_size += mod.new_content.size();
    }
    
    // Limit total modifications to 10MB
    if (total_size > 10 * 1024 * 1024) {
        return false;
    }
    
    // Check for reasonable number of files
    if (modifications.size() > 100) {
        return false; // Too many files to modify at once
    }
    
    return true;
}

bool ResponseParser::checkContentSecurity(const std::string& content, const std::string& file_path) const {
    // Check for potential security risks
    
    // Dangerous patterns in any file type
    static const std::vector<std::string> dangerous_patterns = {
        "system(", "exec(", "eval(", "shell_exec(",
        "rm -rf", "del /f", "format c:",
        "DROP TABLE", "DELETE FROM",
        "__import__", "subprocess.",
        "curl ", "wget ", "download"
    };
    
    std::string lower_content = content;
    std::transform(lower_content.begin(), lower_content.end(), 
                   lower_content.begin(), ::tolower);
    
    for (const auto& pattern : dangerous_patterns) {
        if (lower_content.find(pattern) != std::string::npos) {
            std::cout << "[WARN] Potentially dangerous pattern found: " << pattern 
                      << " in " << file_path << std::endl;
            
            if (m_strict_validation) {
                return false;
            }
        }
    }
    
    // Check for hardcoded credentials
    static const std::vector<std::string> credential_patterns = {
        "password", "secret", "token", "api_key", "private_key"
    };
    
    for (const auto& pattern : credential_patterns) {
        if (lower_content.find(pattern) != std::string::npos &&
            lower_content.find("=") != std::string::npos) {
            std::cout << "[INFO] Potential credential found in " << file_path 
                      << " - please review" << std::endl;
        }
    }
    
    return true;
}

bool ResponseParser::validateFileLimits(const FileModification& modification) const {
    // Check file size
    if (modification.new_content.size() > m_max_file_size) {
        return false;
    }
    
    // Check line count (reasonable limit)
    size_t line_count = std::count(modification.new_content.begin(), 
                                  modification.new_content.end(), '\n');
    if (line_count > 10000) {
        return false; // Too many lines
    }
    
    // Check for reasonable token estimate
    if (modification.estimated_tokens > 50000) {
        return false; // File too complex
    }
    
    return true;
}

} // namespace Camus