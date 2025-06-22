// =================================================================
// include/Camus/SafetyChecker.hpp
// =================================================================
// Header for comprehensive safety checks before code modifications.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Camus {

/**
 * @brief Safety check result
 */
enum class SafetyLevel {
    SAFE,        ///< No safety concerns detected
    WARNING,     ///< Minor safety concerns, proceed with caution
    DANGER,      ///< Major safety concerns, strongly recommend against
    CRITICAL     ///< Critical safety issues, block operation
};

/**
 * @brief Individual safety check result
 */
struct SafetyCheck {
    std::string check_name;       ///< Name of the safety check
    SafetyLevel level;           ///< Safety level
    std::string message;         ///< Descriptive message
    std::string recommendation;  ///< Recommended action
    
    SafetyCheck(const std::string& name, SafetyLevel lvl, 
               const std::string& msg, const std::string& rec = "")
        : check_name(name), level(lvl), message(msg), recommendation(rec) {}
};

/**
 * @brief Comprehensive safety check results
 */
struct SafetyReport {
    SafetyLevel overall_level;            ///< Overall safety assessment
    std::vector<SafetyCheck> checks;      ///< Individual check results
    bool can_proceed;                     ///< Whether operation should proceed
    size_t warning_count;                 ///< Number of warnings
    size_t danger_count;                  ///< Number of dangers
    size_t critical_count;                ///< Number of critical issues
    
    SafetyReport() : overall_level(SafetyLevel::SAFE), can_proceed(true),
                    warning_count(0), danger_count(0), critical_count(0) {}
};

/**
 * @brief Comprehensive safety checker for code modifications
 * 
 * Performs various safety checks before allowing modifications including
 * git status, file permissions, content validation, and more.
 */
class SafetyChecker {
public:
    /**
     * @brief Construct a new SafetyChecker
     * @param project_root Root directory of the project
     */
    explicit SafetyChecker(const std::string& project_root = ".");

    /**
     * @brief Perform comprehensive safety checks
     * @param modifications List of proposed file modifications
     * @param config Configuration settings
     * @return Safety report with all check results
     */
    SafetyReport performSafetyChecks(
        const std::vector<struct FileModification>& modifications,
        const struct AmodifyConfig& config);

    /**
     * @brief Check git working directory status
     * @return Safety check result
     */
    SafetyCheck checkGitStatus();

    /**
     * @brief Check file system permissions
     * @param file_paths Files to check
     * @return Safety check result
     */
    SafetyCheck checkFilePermissions(const std::vector<std::string>& file_paths);

    /**
     * @brief Check total modification size
     * @param modifications List of modifications
     * @param max_size Maximum allowed total size
     * @return Safety check result
     */
    SafetyCheck checkModificationSize(
        const std::vector<struct FileModification>& modifications,
        size_t max_size);

    /**
     * @brief Check for suspicious file patterns
     * @param modifications List of modifications
     * @return Safety check result
     */
    SafetyCheck checkSuspiciousPatterns(
        const std::vector<struct FileModification>& modifications);

    /**
     * @brief Check for dependency conflicts
     * @param modifications List of modifications
     * @return Safety check result
     */
    SafetyCheck checkDependencyConflicts(
        const std::vector<struct FileModification>& modifications);

    /**
     * @brief Check disk space availability
     * @param modifications List of modifications
     * @return Safety check result
     */
    SafetyCheck checkDiskSpace(
        const std::vector<struct FileModification>& modifications);

    /**
     * @brief Check for backup readiness
     * @param config Configuration settings
     * @param file_count Number of files to backup
     * @return Safety check result
     */
    SafetyCheck checkBackupReadiness(const struct AmodifyConfig& config,
                                    size_t file_count);

    /**
     * @brief Set whether to enable strict safety mode
     * @param strict True for strict mode (more restrictive)
     */
    void setStrictMode(bool strict);

    /**
     * @brief Get human-readable safety level description
     * @param level Safety level
     * @return Description string
     */
    static std::string getSafetyLevelDescription(SafetyLevel level);

    /**
     * @brief Get safety level color for terminal display
     * @param level Safety level
     * @return ANSI color code
     */
    static std::string getSafetyLevelColor(SafetyLevel level);

private:
    std::string m_project_root;
    bool m_strict_mode;

    /**
     * @brief Execute system command and get output
     * @param command Command to execute
     * @return Pair of output and exit code
     */
    std::pair<std::string, int> executeCommand(const std::string& command);

    /**
     * @brief Check if path is writable
     * @param path File or directory path
     * @return True if writable
     */
    bool isWritable(const std::string& path);

    /**
     * @brief Get available disk space in bytes
     * @param path Directory path to check
     * @return Available space in bytes
     */
    size_t getAvailableDiskSpace(const std::string& path);

    /**
     * @brief Calculate total size of modifications
     * @param modifications List of modifications
     * @return Total size in bytes
     */
    size_t calculateTotalSize(
        const std::vector<struct FileModification>& modifications);

    /**
     * @brief Update overall safety level based on individual checks
     * @param report Safety report to update
     */
    void updateOverallSafety(SafetyReport& report);
};

} // namespace Camus