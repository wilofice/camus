// =================================================================
// src/Camus/BackupManager.cpp
// =================================================================
// Implementation for managing file backups before modifications.

#include "Camus/BackupManager.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace Camus {

BackupManager::BackupManager(const std::string& backup_dir) 
    : m_backup_dir(backup_dir), m_max_sessions(10) {
    m_session_id = generateSessionId();
    ensureBackupDirectory();
}

std::string BackupManager::createBackup(const std::string& file_path) {
    try {
        // Check if file exists
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "[WARN] Cannot backup non-existent file: " << file_path << std::endl;
            return "";
        }
        
        // Check if already backed up in this session
        if (hasBackup(file_path)) {
            std::cout << "[INFO] File already backed up: " << file_path << std::endl;
            return m_current_backups[file_path].backup_path;
        }
        
        // Generate backup path
        std::string backup_path = generateBackupPath(file_path);
        
        // Ensure backup subdirectory exists
        std::filesystem::path backup_file_path(backup_path);
        std::filesystem::create_directories(backup_file_path.parent_path());
        
        // Copy file to backup location
        if (!copyFile(file_path, backup_path)) {
            std::cerr << "[ERROR] Failed to create backup for: " << file_path << std::endl;
            return "";
        }
        
        // Record backup information
        size_t file_size = std::filesystem::file_size(file_path);
        BackupInfo backup_info(file_path, backup_path, file_size);
        m_current_backups[file_path] = backup_info;
        
        std::cout << "[INFO] Created backup: " << file_path << " -> " << backup_path << std::endl;
        return backup_path;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception creating backup for " << file_path << ": " << e.what() << std::endl;
        return "";
    }
}

size_t BackupManager::createBackups(const std::vector<std::string>& file_paths) {
    size_t successful_backups = 0;
    
    std::cout << "[INFO] Creating backups for " << file_paths.size() << " files..." << std::endl;
    
    for (const auto& file_path : file_paths) {
        if (!createBackup(file_path).empty()) {
            successful_backups++;
        }
    }
    
    std::cout << "[INFO] Successfully created " << successful_backups << " backups" << std::endl;
    return successful_backups;
}

bool BackupManager::restoreFile(const std::string& file_path) {
    auto backup_it = m_current_backups.find(file_path);
    if (backup_it == m_current_backups.end()) {
        std::cerr << "[ERROR] No backup found for: " << file_path << std::endl;
        return false;
    }
    
    const BackupInfo& backup_info = backup_it->second;
    
    try {
        // Copy backup back to original location
        if (!copyFile(backup_info.backup_path, file_path)) {
            std::cerr << "[ERROR] Failed to restore: " << file_path << std::endl;
            return false;
        }
        
        std::cout << "[INFO] Restored file: " << file_path << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception restoring " << file_path << ": " << e.what() << std::endl;
        return false;
    }
}

size_t BackupManager::restoreAllFiles() {
    size_t restored_count = 0;
    
    std::cout << "[INFO] Restoring " << m_current_backups.size() << " files from backup..." << std::endl;
    
    for (const auto& [file_path, backup_info] : m_current_backups) {
        if (restoreFile(file_path)) {
            restored_count++;
        }
    }
    
    std::cout << "[INFO] Successfully restored " << restored_count << " files" << std::endl;
    return restored_count;
}

void BackupManager::cleanupBackups() {
    try {
        for (const auto& [file_path, backup_info] : m_current_backups) {
            if (std::filesystem::exists(backup_info.backup_path)) {
                std::filesystem::remove(backup_info.backup_path);
            }
        }
        
        // Try to remove session directory if empty
        std::string session_dir = m_backup_dir + "/" + m_session_id;
        if (std::filesystem::exists(session_dir) && std::filesystem::is_empty(session_dir)) {
            std::filesystem::remove(session_dir);
        }
        
        m_current_backups.clear();
        std::cout << "[INFO] Cleaned up backups for session: " << m_session_id << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Error cleaning up backups: " << e.what() << std::endl;
    }
}

std::vector<BackupInfo> BackupManager::getBackupInfo() const {
    std::vector<BackupInfo> info_list;
    info_list.reserve(m_current_backups.size());
    
    for (const auto& [file_path, backup_info] : m_current_backups) {
        info_list.push_back(backup_info);
    }
    
    return info_list;
}

bool BackupManager::hasBackup(const std::string& file_path) const {
    return m_current_backups.find(file_path) != m_current_backups.end();
}

void BackupManager::setMaxBackupSessions(size_t max_sessions) {
    m_max_sessions = max_sessions;
}

void BackupManager::cleanOldBackups(size_t max_age_days) {
    try {
        if (!std::filesystem::exists(m_backup_dir)) {
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto max_age = std::chrono::hours(24 * max_age_days);
        
        size_t removed_sessions = 0;
        
        for (const auto& entry : std::filesystem::directory_iterator(m_backup_dir)) {
            if (!entry.is_directory()) {
                continue;
            }
            
            auto last_write = std::filesystem::last_write_time(entry.path());
            auto file_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                last_write - std::filesystem::file_time_type::clock::now() + 
                std::chrono::system_clock::now());
            
            if (max_age_days > 0 && (now - file_time) > max_age) {
                std::filesystem::remove_all(entry.path());
                removed_sessions++;
            }
        }
        
        if (removed_sessions > 0) {
            std::cout << "[INFO] Cleaned up " << removed_sessions << " old backup sessions" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Error cleaning old backups: " << e.what() << std::endl;
    }
}

std::string BackupManager::generateSessionId() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    // Add milliseconds for uniqueness
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    oss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

bool BackupManager::ensureBackupDirectory() {
    try {
        std::filesystem::create_directories(m_backup_dir);
        std::filesystem::create_directories(m_backup_dir + "/" + m_session_id);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to create backup directory: " << e.what() << std::endl;
        return false;
    }
}

std::string BackupManager::generateBackupPath(const std::string& original_path) const {
    // Convert file path to backup path
    std::string backup_path = original_path;
    
    // Replace path separators with underscores for backup filename
    std::replace(backup_path.begin(), backup_path.end(), '/', '_');
    std::replace(backup_path.begin(), backup_path.end(), '\\', '_');
    
    return m_backup_dir + "/" + m_session_id + "/" + backup_path;
}

bool BackupManager::copyFile(const std::string& source_path, const std::string& dest_path) const {
    try {
        std::filesystem::copy_file(source_path, dest_path, 
                                  std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to copy " << source_path << " to " << dest_path 
                  << ": " << e.what() << std::endl;
        return false;
    }
}

} // namespace Camus