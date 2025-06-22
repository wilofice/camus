// =================================================================
// src/Camus/ProjectScanner.cpp
// =================================================================
// Implementation for project file discovery and filtering.

#include "Camus/ProjectScanner.hpp"
#include "Camus/IgnorePattern.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Camus {

ProjectScanner::ProjectScanner(const std::string& root_path) 
    : m_root_path(std::filesystem::absolute(root_path).string()),
      m_max_file_size(1024 * 1024) // 1MB default
{
    initializeDefaults();
    loadCamusIgnore();
}

std::vector<std::string> ProjectScanner::scanFiles() {
    std::vector<std::string> discovered_files;
    
    try {
        // Use recursive directory iterator to walk the file tree
        for (const auto& entry : std::filesystem::recursive_directory_iterator(m_root_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            std::string relative_path = getRelativePath(entry.path());
            
            // Apply ignore patterns
            bool is_directory = entry.is_directory();
            if (m_ignore_patterns.shouldIgnore(relative_path, is_directory)) {
                continue;
            }
            
            // Check file extension
            std::string extension = entry.path().extension().string();
            if (m_include_extensions.find(extension) == m_include_extensions.end()) {
                continue;
            }
            
            // Check file size
            if (entry.file_size() > m_max_file_size) {
                std::cout << "[WARN] Skipping large file: " << relative_path 
                         << " (" << entry.file_size() << " bytes)" << std::endl;
                continue;
            }
            
            // Check if file is text (not binary)
            if (!isTextFile(entry.path().string())) {
                continue;
            }
            
            discovered_files.push_back(relative_path);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[ERROR] Filesystem error while scanning: " << e.what() << std::endl;
    }
    
    // Sort files alphabetically for consistent output
    std::sort(discovered_files.begin(), discovered_files.end());
    
    std::cout << "[INFO] Discovered " << discovered_files.size() << " relevant files" << std::endl;
    return discovered_files;
}

void ProjectScanner::addIgnorePattern(const std::string& pattern) {
    m_ignore_patterns.addPattern(pattern);
}

void ProjectScanner::addIncludeExtension(const std::string& extension) {
    m_include_extensions.insert(extension);
}

void ProjectScanner::setMaxFileSize(size_t max_size) {
    m_max_file_size = max_size;
}

bool ProjectScanner::shouldIgnoreFile(const std::string& relative_path) const {
    // This method is now redundant since we use IgnorePatternSet directly
    // but keeping for backward compatibility
    return m_ignore_patterns.shouldIgnore(relative_path, false);
}

bool ProjectScanner::isTextFile(const std::string& file_path) const {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read first 512 bytes to check for null bytes (binary indicator)
    constexpr size_t sample_size = 512;
    char buffer[sample_size];
    file.read(buffer, sample_size);
    size_t bytes_read = file.gcount();
    
    // Check for null bytes (common in binary files)
    for (size_t i = 0; i < bytes_read; ++i) {
        if (buffer[i] == '\0') {
            return false;
        }
    }
    
    // Check for reasonable text content ratio
    size_t printable_chars = 0;
    for (size_t i = 0; i < bytes_read; ++i) {
        if (std::isprint(static_cast<unsigned char>(buffer[i])) || 
            std::isspace(static_cast<unsigned char>(buffer[i]))) {
            printable_chars++;
        }
    }
    
    // If more than 95% of characters are printable, consider it text
    return bytes_read > 0 && (static_cast<double>(printable_chars) / bytes_read) > 0.95;
}

void ProjectScanner::loadCamusIgnore() {
    std::string ignore_file_path = m_root_path + "/.camusignore";
    size_t patterns_loaded = m_ignore_patterns.loadFromFile(ignore_file_path);
    
    if (patterns_loaded > 0) {
        std::cout << "[INFO] Loaded .camusignore with " << patterns_loaded << " patterns" << std::endl;
    }
}

void ProjectScanner::initializeDefaults() {
    // Default ignore patterns
    std::vector<std::string> default_patterns = {
        ".git/",
        ".git",
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
        "*~"
    };
    
    for (const auto& pattern : default_patterns) {
        m_ignore_patterns.addPattern(pattern);
    }
    
    // Default include extensions
    m_include_extensions = {
        ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hxx",
        ".py", ".pyx", ".pyi",
        ".js", ".jsx", ".ts", ".tsx", ".mjs",
        ".java", ".scala", ".kt",
        ".rs", ".go", ".rb", ".php",
        ".md", ".txt", ".rst",
        ".yml", ".yaml", ".json", ".toml", ".ini", ".cfg",
        ".xml", ".html", ".css", ".scss", ".sass",
        ".sh", ".bash", ".zsh", ".fish",
        ".sql", ".cmake", ".make", ".mk",
        ".dockerfile", ".dockerignore",
        ".gitignore", ".gitattributes"
    };
}


std::string ProjectScanner::getRelativePath(const std::filesystem::path& abs_path) const {
    std::filesystem::path root(m_root_path);
    std::filesystem::path relative = std::filesystem::relative(abs_path, root);
    return relative.string();
}

} // namespace Camus