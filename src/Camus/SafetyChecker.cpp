// =================================================================
// src/Camus/SafetyChecker.cpp
// =================================================================
// Implementation for comprehensive safety checks.

#include "Camus/SafetyChecker.hpp"
#include "Camus/ResponseParser.hpp"
#include "Camus/AmodifyConfig.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <regex>
#include <array>
#include <memory>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#endif

namespace Camus {

SafetyChecker::SafetyChecker(const std::string& project_root)
    : m_project_root(std::filesystem::absolute(project_root).string()),
      m_strict_mode(false) {
}

SafetyReport SafetyChecker::performSafetyChecks(
    const std::vector<FileModification>& modifications,
    const AmodifyConfig& config) {
    
    SafetyReport report;
    
    std::cout << "Performing safety checks..." << std::endl;
    
    // Git status check
    if (config.git_check) {
        auto git_check = checkGitStatus();
        report.checks.push_back(git_check);
    }
    
    // Extract file paths for permission checking
    std::vector<std::string> file_paths;
    for (const auto& mod : modifications) {
        file_paths.push_back(mod.file_path);
    }
    
    // File permissions check
    auto perm_check = checkFilePermissions(file_paths);
    report.checks.push_back(perm_check);
    
    // Modification size check
    auto size_check = checkModificationSize(modifications, config.max_modification_size);
    report.checks.push_back(size_check);
    
    // Suspicious patterns check
    auto pattern_check = checkSuspiciousPatterns(modifications);
    report.checks.push_back(pattern_check);
    
    // Dependency conflicts check
    auto dep_check = checkDependencyConflicts(modifications);
    report.checks.push_back(dep_check);
    
    // Disk space check
    auto disk_check = checkDiskSpace(modifications);
    report.checks.push_back(disk_check);
    
    // Backup readiness check
    if (config.create_backups) {
        size_t files_to_backup = 0;
        for (const auto& mod : modifications) {
            if (!mod.is_new_file) files_to_backup++;
        }
        auto backup_check = checkBackupReadiness(config, files_to_backup);
        report.checks.push_back(backup_check);
    }
    
    // Count issues and determine overall safety
    for (const auto& check : report.checks) {
        switch (check.level) {
            case SafetyLevel::WARNING:
                report.warning_count++;
                break;
            case SafetyLevel::DANGER:
                report.danger_count++;
                break;
            case SafetyLevel::CRITICAL:
                report.critical_count++;
                break;
            default:
                break;
        }
    }
    
    updateOverallSafety(report);
    
    return report;
}

SafetyCheck SafetyChecker::checkGitStatus() {
    auto [output, exit_code] = executeCommand("git status --porcelain");
    
    if (exit_code != 0) {
        return SafetyCheck("Git Status", SafetyLevel::WARNING,
                          "Not in a git repository or git not available",
                          "Ensure you're in a git repository if version control is important");
    }
    
    if (!output.empty()) {
        // Count different types of changes
        std::istringstream iss(output);
        std::string line;
        int uncommitted = 0, untracked = 0;
        
        while (std::getline(iss, line)) {
            if (line.length() >= 2) {
                if (line.substr(0, 2) == "??") {
                    untracked++;
                } else {
                    uncommitted++;
                }
            }
        }
        
        std::ostringstream msg;
        msg << "Working directory not clean: ";
        if (uncommitted > 0) msg << uncommitted << " uncommitted changes ";
        if (untracked > 0) msg << untracked << " untracked files";
        
        SafetyLevel level = m_strict_mode ? SafetyLevel::DANGER : SafetyLevel::WARNING;
        return SafetyCheck("Git Status", level, msg.str(),
                          "Commit or stash changes before major modifications");
    }
    
    return SafetyCheck("Git Status", SafetyLevel::SAFE,
                      "Working directory is clean");
}

SafetyCheck SafetyChecker::checkFilePermissions(const std::vector<std::string>& file_paths) {
    std::vector<std::string> problematic_files;
    
    for (const auto& file_path : file_paths) {
        std::filesystem::path abs_path = std::filesystem::path(m_project_root) / file_path;
        
        // Check if parent directory is writable for new files
        if (!std::filesystem::exists(abs_path)) {
            std::filesystem::path parent = abs_path.parent_path();
            if (!isWritable(parent.string())) {
                problematic_files.push_back(file_path + " (parent dir not writable)");
            }
        } else {
            // Check if existing file is writable
            if (!isWritable(abs_path.string())) {
                problematic_files.push_back(file_path + " (read-only)");
            }
        }
    }
    
    if (!problematic_files.empty()) {
        std::ostringstream msg;
        msg << "Permission issues with " << problematic_files.size() << " files: ";
        for (size_t i = 0; i < std::min(size_t(3), problematic_files.size()); i++) {
            if (i > 0) msg << ", ";
            msg << problematic_files[i];
        }
        if (problematic_files.size() > 3) {
            msg << " and " << (problematic_files.size() - 3) << " more";
        }
        
        return SafetyCheck("File Permissions", SafetyLevel::CRITICAL, msg.str(),
                          "Fix file permissions before proceeding");
    }
    
    return SafetyCheck("File Permissions", SafetyLevel::SAFE,
                      "All files are writable");
}

SafetyCheck SafetyChecker::checkModificationSize(
    const std::vector<FileModification>& modifications,
    size_t max_size) {
    
    size_t total_size = calculateTotalSize(modifications);
    
    if (total_size > max_size) {
        std::ostringstream msg;
        msg << "Total modification size (" << (total_size / 1024 / 1024) 
            << "MB) exceeds limit (" << (max_size / 1024 / 1024) << "MB)";
        
        return SafetyCheck("Modification Size", SafetyLevel::DANGER, msg.str(),
                          "Consider reducing scope or increasing size limit");
    }
    
    if (total_size > max_size / 2) {
        std::ostringstream msg;
        msg << "Large modification size: " << (total_size / 1024 / 1024) << "MB";
        
        return SafetyCheck("Modification Size", SafetyLevel::WARNING, msg.str(),
                          "Review changes carefully");
    }
    
    return SafetyCheck("Modification Size", SafetyLevel::SAFE,
                      "Modification size is reasonable");
}

SafetyCheck SafetyChecker::checkSuspiciousPatterns(
    const std::vector<FileModification>& modifications) {
    
    std::vector<std::string> suspicious_files;
    
    // Patterns that might indicate problematic content
    std::vector<std::regex> dangerous_patterns = {
        std::regex(R"(rm\s+-rf\s+[/*])", std::regex_constants::icase),
        std::regex(R"(format\s+c:|del\s+/f)", std::regex_constants::icase),
        std::regex(R"(DROP\s+DATABASE|TRUNCATE\s+TABLE)", std::regex_constants::icase),
        std::regex(R"(eval\s*\(|exec\s*\()", std::regex_constants::icase),
        std::regex(R"(system\s*\(.*["'].*rm|del)", std::regex_constants::icase)
    };
    
    for (const auto& mod : modifications) {
        for (const auto& pattern : dangerous_patterns) {
            if (std::regex_search(mod.new_content, pattern)) {
                suspicious_files.push_back(mod.file_path);
                break;
            }
        }
        
        // Check for hardcoded credentials
        std::regex cred_pattern(R"((password|secret|token|key)\s*[:=]\s*['"]\w+)", 
                               std::regex_constants::icase);
        if (std::regex_search(mod.new_content, cred_pattern)) {
            suspicious_files.push_back(mod.file_path + " (potential credentials)");
        }
    }
    
    if (!suspicious_files.empty()) {
        std::ostringstream msg;
        msg << "Suspicious patterns found in " << suspicious_files.size() << " files";
        
        SafetyLevel level = m_strict_mode ? SafetyLevel::CRITICAL : SafetyLevel::DANGER;
        return SafetyCheck("Suspicious Patterns", level, msg.str(),
                          "Review content carefully before applying");
    }
    
    return SafetyCheck("Suspicious Patterns", SafetyLevel::SAFE,
                      "No suspicious patterns detected");
}

SafetyCheck SafetyChecker::checkDependencyConflicts(
    const std::vector<FileModification>& modifications) {
    
    // Look for potential dependency issues
    std::vector<std::string> potential_conflicts;
    
    // Check for modifications to critical files
    std::vector<std::string> critical_files = {
        "CMakeLists.txt", "package.json", "requirements.txt", 
        "Cargo.toml", "pom.xml", "build.gradle"
    };
    
    for (const auto& mod : modifications) {
        for (const auto& critical : critical_files) {
            if (mod.file_path.find(critical) != std::string::npos) {
                potential_conflicts.push_back(mod.file_path);
            }
        }
    }
    
    if (!potential_conflicts.empty()) {
        std::ostringstream msg;
        msg << "Modifications to dependency files: ";
        for (size_t i = 0; i < std::min(size_t(3), potential_conflicts.size()); i++) {
            if (i > 0) msg << ", ";
            msg << potential_conflicts[i];
        }
        
        return SafetyCheck("Dependency Conflicts", SafetyLevel::WARNING, msg.str(),
                          "Verify dependencies after applying changes");
    }
    
    return SafetyCheck("Dependency Conflicts", SafetyLevel::SAFE,
                      "No dependency conflicts detected");
}

SafetyCheck SafetyChecker::checkDiskSpace(
    const std::vector<FileModification>& modifications) {
    
    size_t required_space = calculateTotalSize(modifications);
    size_t available_space = getAvailableDiskSpace(m_project_root);
    
    // Add buffer for backups (assume 2x space needed)
    size_t needed_space = required_space * 2;
    
    if (available_space < needed_space) {
        std::ostringstream msg;
        msg << "Insufficient disk space. Need ~" << (needed_space / 1024 / 1024) 
            << "MB, have " << (available_space / 1024 / 1024) << "MB";
        
        return SafetyCheck("Disk Space", SafetyLevel::CRITICAL, msg.str(),
                          "Free up disk space before proceeding");
    }
    
    if (available_space < needed_space * 2) {
        return SafetyCheck("Disk Space", SafetyLevel::WARNING,
                          "Disk space is getting low",
                          "Monitor disk usage during operation");
    }
    
    return SafetyCheck("Disk Space", SafetyLevel::SAFE,
                      "Sufficient disk space available");
}

SafetyCheck SafetyChecker::checkBackupReadiness(const AmodifyConfig& config,
                                               size_t file_count) {
    
    if (!config.create_backups) {
        return SafetyCheck("Backup Readiness", SafetyLevel::SAFE,
                          "Backups disabled");
    }
    
    // Check if backup directory can be created
    std::filesystem::path backup_path(config.backup_dir);
    
    try {
        std::filesystem::create_directories(backup_path);
    } catch (const std::exception& e) {
        return SafetyCheck("Backup Readiness", SafetyLevel::CRITICAL,
                          "Cannot create backup directory: " + std::string(e.what()),
                          "Fix backup directory permissions or change backup location");
    }
    
    if (!isWritable(backup_path.string())) {
        return SafetyCheck("Backup Readiness", SafetyLevel::CRITICAL,
                          "Backup directory is not writable",
                          "Fix backup directory permissions");
    }
    
    return SafetyCheck("Backup Readiness", SafetyLevel::SAFE,
                      "Backup system is ready");
}

void SafetyChecker::setStrictMode(bool strict) {
    m_strict_mode = strict;
}

std::string SafetyChecker::getSafetyLevelDescription(SafetyLevel level) {
    switch (level) {
        case SafetyLevel::SAFE: return "Safe";
        case SafetyLevel::WARNING: return "Warning";
        case SafetyLevel::DANGER: return "Danger";
        case SafetyLevel::CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

std::string SafetyChecker::getSafetyLevelColor(SafetyLevel level) {
    switch (level) {
        case SafetyLevel::SAFE: return "\033[32m";      // Green
        case SafetyLevel::WARNING: return "\033[33m";   // Yellow
        case SafetyLevel::DANGER: return "\033[31m";    // Red
        case SafetyLevel::CRITICAL: return "\033[91m";  // Bright Red
        default: return "\033[0m";                      // Reset
    }
}

std::pair<std::string, int> SafetyChecker::executeCommand(const std::string& command) {
    std::string result;
    std::array<char, 128> buffer;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return {"", -1};
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    int exit_code = pclose(pipe.release());
    return {result, exit_code};
}

bool SafetyChecker::isWritable(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        
        if (std::filesystem::exists(fs_path)) {
            // For existing files/directories
            auto perms = std::filesystem::status(fs_path).permissions();
            return (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
        } else {
            // For non-existent paths, check parent directory
            auto parent = fs_path.parent_path();
            if (std::filesystem::exists(parent)) {
                auto perms = std::filesystem::status(parent).permissions();
                return (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
            }
        }
    } catch (...) {
        return false;
    }
    
    return false;
}

size_t SafetyChecker::getAvailableDiskSpace(const std::string& path) {
    try {
#if defined(_WIN32)
        ULARGE_INTEGER free_bytes;
        if (GetDiskFreeSpaceExA(path.c_str(), &free_bytes, nullptr, nullptr)) {
            return static_cast<size_t>(free_bytes.QuadPart);
        }
#else
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) == 0) {
            return static_cast<size_t>(stat.f_bavail) * stat.f_frsize;
        }
#endif
    } catch (...) {
        // Return a conservative estimate if we can't get actual space
        return 1024 * 1024 * 1024; // 1GB
    }
    
    return 1024 * 1024 * 1024; // 1GB default
}

size_t SafetyChecker::calculateTotalSize(
    const std::vector<FileModification>& modifications) {
    
    size_t total = 0;
    for (const auto& mod : modifications) {
        total += mod.new_content.size();
    }
    return total;
}

void SafetyChecker::updateOverallSafety(SafetyReport& report) {
    if (report.critical_count > 0) {
        report.overall_level = SafetyLevel::CRITICAL;
        report.can_proceed = false;
    } else if (report.danger_count > 0) {
        report.overall_level = SafetyLevel::DANGER;
        report.can_proceed = !m_strict_mode; // Block in strict mode
    } else if (report.warning_count > 0) {
        report.overall_level = SafetyLevel::WARNING;
        report.can_proceed = true;
    } else {
        report.overall_level = SafetyLevel::SAFE;
        report.can_proceed = true;
    }
}

} // namespace Camus