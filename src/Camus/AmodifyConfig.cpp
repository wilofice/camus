// =================================================================
// src/Camus/AmodifyConfig.cpp
// =================================================================
// Implementation for amodify configuration management.

#include "Camus/AmodifyConfig.hpp"
#include "Camus/ConfigParser.hpp"
#include "Camus/CliParser.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace Camus {

void AmodifyConfig::loadFromConfig(const ConfigParser& config) {
    // Load simple values with defaults
    std::string max_files_str = config.getStringValue("amodify.max_files");
    if (!max_files_str.empty()) {
        try {
            max_files = std::stoul(max_files_str);
        } catch (...) {
            std::cerr << "[WARN] Invalid amodify.max_files value, using default" << std::endl;
        }
    }
    
    std::string max_tokens_str = config.getStringValue("amodify.max_tokens");
    if (!max_tokens_str.empty()) {
        try {
            max_tokens = std::stoul(max_tokens_str);
        } catch (...) {
            std::cerr << "[WARN] Invalid amodify.max_tokens value, using default" << std::endl;
        }
    }
    
    std::string create_backups_str = config.getStringValue("amodify.create_backups");
    if (!create_backups_str.empty()) {
        create_backups = (create_backups_str == "true" || create_backups_str == "1");
    }
    
    std::string backup_dir_str = config.getStringValue("amodify.backup_dir");
    if (!backup_dir_str.empty()) {
        backup_dir = backup_dir_str;
    }
    
    std::string interactive_threshold_str = config.getStringValue("amodify.interactive_threshold");
    if (!interactive_threshold_str.empty()) {
        try {
            interactive_threshold = std::stoul(interactive_threshold_str);
        } catch (...) {
            std::cerr << "[WARN] Invalid amodify.interactive_threshold value, using default" << std::endl;
        }
    }
    
    std::string git_check_str = config.getStringValue("amodify.git_check");
    if (!git_check_str.empty()) {
        git_check = (git_check_str == "true" || git_check_str == "1");
    }
    
    // For arrays, we'll need to parse them manually from the config
    // Since the current ConfigParser doesn't support arrays, we'll use defaults
    // In a full implementation, we'd enhance ConfigParser to support YAML arrays
    
    // Use defaults if not configured
    if (include_extensions.empty()) {
        include_extensions = getDefaultExtensions();
    }
    
    if (default_ignore_patterns.empty()) {
        default_ignore_patterns = getDefaultIgnorePatterns();
    }
}

void AmodifyConfig::applyCommandOverrides(const Commands& commands) {
    // Override with command-line options if provided
    if (commands.max_files != 100) { // Check if non-default
        max_files = commands.max_files;
    }
    
    if (commands.max_tokens != 128000) { // Check if non-default
        max_tokens = commands.max_tokens;
    }
    
    // Note: include_pattern and exclude_pattern from commands would need
    // special handling to merge with configured patterns
}

std::unordered_set<std::string> AmodifyConfig::getMergedExtensions() const {
    std::unordered_set<std::string> extensions;
    
    // Add configured extensions
    for (const auto& ext : include_extensions) {
        extensions.insert(ext);
    }
    
    // If no extensions configured, use defaults
    if (extensions.empty()) {
        auto defaults = getDefaultExtensions();
        extensions.insert(defaults.begin(), defaults.end());
    }
    
    return extensions;
}

std::vector<std::string> AmodifyConfig::getMergedIgnorePatterns() const {
    std::vector<std::string> patterns;
    
    // Start with configured patterns
    patterns = default_ignore_patterns;
    
    // If no patterns configured, use defaults
    if (patterns.empty()) {
        patterns = getDefaultIgnorePatterns();
    }
    
    return patterns;
}

bool AmodifyConfig::validate() const {
    bool valid = true;
    
    if (max_files == 0) {
        std::cerr << "[ERROR] max_files must be greater than 0" << std::endl;
        valid = false;
    }
    
    if (max_tokens < 1000) {
        std::cerr << "[ERROR] max_tokens must be at least 1000" << std::endl;
        valid = false;
    }
    
    if (backup_dir.empty()) {
        std::cerr << "[ERROR] backup_dir cannot be empty" << std::endl;
        valid = false;
    }
    
    if (max_modification_size == 0) {
        std::cerr << "[ERROR] max_modification_size must be greater than 0" << std::endl;
        valid = false;
    }
    
    return valid;
}

std::vector<std::string> AmodifyConfig::getDefaultExtensions() {
    return {
        ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hxx",
        ".py", ".pyx", ".pyi",
        ".js", ".jsx", ".ts", ".tsx", ".mjs",
        ".java", ".scala", ".kt",
        ".rs", ".go", ".rb", ".php",
        ".md", ".txt", ".rst",
        ".yml", ".yaml", ".json", ".toml", ".ini", ".cfg",
        ".xml", ".cmake", ".make", ".mk",
        ".sh", ".bash", ".zsh", ".fish",
        ".sql"
    };
}

std::vector<std::string> AmodifyConfig::getDefaultIgnorePatterns() {
    return {
        ".git/",
        "build/",
        "cmake-build-*/",
        "node_modules/",
        "__pycache__/",
        "*.pyc",
        "*.pyo",
        "*.o",
        "*.obj",
        "*.a",
        "*.lib",
        "*.so",
        "*.dll",
        "*.dylib",
        "*.exe",
        ".DS_Store",
        "Thumbs.db",
        "*.tmp",
        "*.temp",
        "*.log",
        ".vscode/",
        ".idea/",
        "*.swp",
        "*.swo",
        "*~",
        ".camus/"
    };
}

} // namespace Camus