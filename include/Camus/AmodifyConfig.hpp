// =================================================================
// include/Camus/AmodifyConfig.hpp
// =================================================================
// Configuration structure for amodify command settings.

#pragma once

#include <string>
#include <vector>
#include <unordered_set>

namespace Camus {

/**
 * @brief Configuration settings for the amodify command
 */
struct AmodifyConfig {
    // File discovery settings
    size_t max_files = 100;
    size_t max_tokens = 128000;
    std::vector<std::string> include_extensions;
    std::vector<std::string> default_ignore_patterns;
    
    // Backup settings
    bool create_backups = true;
    std::string backup_dir = ".camus/backups";
    
    // UI settings
    size_t interactive_threshold = 5;
    
    // Safety settings
    bool git_check = true;
    bool dry_run = false;
    size_t max_modification_size = 10 * 1024 * 1024; // 10MB total
    
    /**
     * @brief Load configuration from ConfigParser
     * @param config ConfigParser instance
     */
    void loadFromConfig(const class ConfigParser& config);
    
    /**
     * @brief Apply command-line overrides
     * @param commands Command-line arguments
     */
    void applyCommandOverrides(const struct Commands& commands);
    
    /**
     * @brief Get merged include extensions (config + defaults)
     * @return Set of file extensions to include
     */
    std::unordered_set<std::string> getMergedExtensions() const;
    
    /**
     * @brief Get merged ignore patterns (config + defaults)
     * @return Vector of ignore patterns
     */
    std::vector<std::string> getMergedIgnorePatterns() const;
    
    /**
     * @brief Validate configuration settings
     * @return True if configuration is valid
     */
    bool validate() const;
    
    /**
     * @brief Get default extensions if none configured
     * @return Default file extensions
     */
    static std::vector<std::string> getDefaultExtensions();
    
    /**
     * @brief Get default ignore patterns
     * @return Default ignore patterns
     */
    static std::vector<std::string> getDefaultIgnorePatterns();
};

} // namespace Camus