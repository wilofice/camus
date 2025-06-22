// =================================================================
// include/Camus/BackupManager.hpp
// =================================================================
// Header for managing file backups before modifications.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace Camus {

/**
 * @brief Information about a single backup
 */
struct BackupInfo {
    std::string original_path;     ///< Original file path
    std::string backup_path;       ///< Backup file path
    std::chrono::system_clock::time_point created_at; ///< Backup creation time
    size_t file_size;             ///< Original file size
    
    BackupInfo() : file_size(0) {}
    BackupInfo(const std::string& orig, const std::string& backup, size_t size)
        : original_path(orig), backup_path(backup), file_size(size), 
          created_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief Manages file backups and restoration for safe modifications
 * 
 * The BackupManager creates backups of files before they are modified,
 * allowing for safe rollback in case of errors or user cancellation.
 */
class BackupManager {
public:
    /**
     * @brief Construct a new BackupManager
     * @param backup_dir Directory to store backups (default: .camus/backups)
     */
    explicit BackupManager(const std::string& backup_dir = ".camus/backups");

    /**
     * @brief Create backup of a file before modification
     * @param file_path Path to file to backup
     * @return Backup path on success, empty string on failure
     */
    std::string createBackup(const std::string& file_path);

    /**
     * @brief Create backups for multiple files
     * @param file_paths Vector of file paths to backup
     * @return Number of successful backups created
     */
    size_t createBackups(const std::vector<std::string>& file_paths);

    /**
     * @brief Restore a file from backup
     * @param file_path Original file path
     * @return True if restoration successful
     */
    bool restoreFile(const std::string& file_path);

    /**
     * @brief Restore all files from current backup session
     * @return Number of files successfully restored
     */
    size_t restoreAllFiles();

    /**
     * @brief Clean up backups after successful modifications
     */
    void cleanupBackups();

    /**
     * @brief Get information about all current backups
     * @return Vector of backup information
     */
    std::vector<BackupInfo> getBackupInfo() const;

    /**
     * @brief Check if a file has a backup in current session
     * @param file_path Original file path
     * @return True if backup exists
     */
    bool hasBackup(const std::string& file_path) const;

    /**
     * @brief Set maximum number of backup sessions to keep
     * @param max_sessions Maximum sessions (0 = unlimited)
     */
    void setMaxBackupSessions(size_t max_sessions);

    /**
     * @brief Clean old backup sessions
     * @param max_age_days Maximum age in days (0 = no age limit)
     */
    void cleanOldBackups(size_t max_age_days = 7);

private:
    std::string m_backup_dir;
    std::string m_session_id;
    std::unordered_map<std::string, BackupInfo> m_current_backups;
    size_t m_max_sessions;

    /**
     * @brief Generate unique session ID for this backup session
     * @return Session ID string
     */
    std::string generateSessionId() const;

    /**
     * @brief Ensure backup directory exists
     * @return True if directory exists or was created
     */
    bool ensureBackupDirectory();

    /**
     * @brief Generate backup file path for a given file
     * @param original_path Original file path
     * @return Backup file path
     */
    std::string generateBackupPath(const std::string& original_path) const;

    /**
     * @brief Copy file with metadata preservation
     * @param source_path Source file
     * @param dest_path Destination file
     * @return True if copy successful
     */
    bool copyFile(const std::string& source_path, const std::string& dest_path) const;
};

} // namespace Camus