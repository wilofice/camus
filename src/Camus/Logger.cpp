// =================================================================
// src/Camus/Logger.cpp
// =================================================================
// Implementation for comprehensive logging system.

#include "Camus/Logger.hpp"
#include "Camus/ResponseParser.hpp"
#include "Camus/SafetyChecker.hpp"
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace Camus {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    flush();
}

void Logger::initialize(const std::string& log_dir, size_t max_log_size, size_t max_log_files) {
    m_log_dir = log_dir;
    m_max_log_size = max_log_size;
    m_max_log_files = max_log_files;
    m_console_level = LogLevel::INFO;
    m_file_level = LogLevel::DEBUG;
    m_console_enabled = true;
    m_current_log_size = 0;
    m_initialized = true;
    
    ensureLogDirectory();
    
    // Create initial log file
    m_current_log_filename = generateLogFilename();
    m_current_log_file = std::make_unique<std::ofstream>(m_current_log_filename, std::ios::app);
    
    info("Logger", "Logging system initialized", m_log_dir);
}

void Logger::setConsoleLogLevel(LogLevel level) {
    m_console_level = level;
}

void Logger::setFileLogLevel(LogLevel level) {
    m_file_level = level;
}

void Logger::setConsoleLogging(bool enabled) {
    m_console_enabled = enabled;
}

void Logger::debug(const std::string& component, const std::string& message, const std::string& context) {
    logEntry(LogEntry(LogLevel::DEBUG, component, message, context));
}

void Logger::info(const std::string& component, const std::string& message, const std::string& context) {
    logEntry(LogEntry(LogLevel::INFO, component, message, context));
}

void Logger::warning(const std::string& component, const std::string& message, const std::string& context) {
    logEntry(LogEntry(LogLevel::WARNING, component, message, context));
}

void Logger::error(const std::string& component, const std::string& message, const std::string& context) {
    logEntry(LogEntry(LogLevel::ERROR, component, message, context));
}

void Logger::critical(const std::string& component, const std::string& message, const std::string& context) {
    logEntry(LogEntry(LogLevel::CRITICAL, component, message, context));
}

void Logger::logFileDiscovery(const std::vector<std::string>& discovered_files,
                             const std::vector<std::string>& filtered_files) {
    std::ostringstream context;
    context << "Discovered: " << discovered_files.size() << " files, ";
    context << "After filtering: " << filtered_files.size() << " files";
    
    info("ProjectScanner", "File discovery completed", context.str());
    
    // Log first few files for debugging
    if (!filtered_files.empty()) {
        std::ostringstream file_list;
        for (size_t i = 0; i < std::min(size_t(10), filtered_files.size()); i++) {
            if (i > 0) file_list << ", ";
            file_list << filtered_files[i];
        }
        if (filtered_files.size() > 10) {
            file_list << " and " << (filtered_files.size() - 10) << " more";
        }
        debug("ProjectScanner", "Files included: " + file_list.str());
    }
}

void Logger::logContextBuilding(size_t total_files, size_t included_files,
                               size_t tokens_used, size_t truncated_files) {
    std::ostringstream context;
    context << "Total: " << total_files << ", ";
    context << "Included: " << included_files << ", ";
    context << "Tokens: " << tokens_used << ", ";
    context << "Truncated: " << truncated_files;
    
    info("ContextBuilder", "Context building completed", context.str());
    
    if (truncated_files > 0) {
        warning("ContextBuilder", 
               "Some files were truncated due to token limits",
               "Files truncated: " + std::to_string(truncated_files));
    }
}

void Logger::logLlmInteraction(size_t request_tokens, size_t response_size,
                              long duration_ms, bool success) {
    std::ostringstream context;
    context << "Request tokens: " << request_tokens << ", ";
    context << "Response size: " << response_size << " chars, ";
    context << "Duration: " << duration_ms << "ms";
    
    if (success) {
        info("LLM", "Request completed successfully", context.str());
    } else {
        error("LLM", "Request failed", context.str());
    }
    
    // Log performance metrics
    if (duration_ms > 30000) { // > 30 seconds
        warning("LLM", "Slow response detected", "Duration: " + std::to_string(duration_ms) + "ms");
    }
}

void Logger::logFileModifications(const std::vector<FileModification>& modifications,
                                 size_t applied_count, size_t failed_count) {
    std::ostringstream context;
    context << "Total: " << modifications.size() << ", ";
    context << "Applied: " << applied_count << ", ";
    context << "Failed: " << failed_count;
    
    if (failed_count == 0) {
        info("FileModifier", "All modifications applied successfully", context.str());
    } else {
        error("FileModifier", "Some modifications failed", context.str());
    }
    
    // Log details for each modification
    for (const auto& mod : modifications) {
        std::ostringstream mod_context;
        mod_context << "Size: " << mod.new_content.size() << " bytes, ";
        mod_context << "Type: " << (mod.is_new_file ? "new" : "modify");
        
        debug("FileModifier", "Processing: " + mod.file_path, mod_context.str());
    }
}

void Logger::logSafetyChecks(const SafetyReport& safety_report) {
    std::ostringstream context;
    context << "Overall: " << SafetyChecker::getSafetyLevelDescription(safety_report.overall_level) << ", ";
    context << "Warnings: " << safety_report.warning_count << ", ";
    context << "Dangers: " << safety_report.danger_count << ", ";
    context << "Critical: " << safety_report.critical_count;
    
    LogLevel log_level;
    switch (safety_report.overall_level) {
        case SafetyLevel::CRITICAL:
            log_level = LogLevel::CRITICAL;
            break;
        case SafetyLevel::DANGER:
            log_level = LogLevel::ERROR;
            break;
        case SafetyLevel::WARNING:
            log_level = LogLevel::WARNING;
            break;
        default:
            log_level = LogLevel::INFO;
            break;
    }
    
    logEntry(LogEntry(log_level, "SafetyChecker", "Safety checks completed", context.str()));
    
    // Log individual check results
    for (const auto& check : safety_report.checks) {
        LogLevel check_level;
        switch (check.level) {
            case SafetyLevel::CRITICAL:
                check_level = LogLevel::CRITICAL;
                break;
            case SafetyLevel::DANGER:
                check_level = LogLevel::ERROR;
                break;
            case SafetyLevel::WARNING:
                check_level = LogLevel::WARNING;
                break;
            default:
                check_level = LogLevel::DEBUG;
                break;
        }
        
        logEntry(LogEntry(check_level, "SafetyCheck", 
                 check.check_name + ": " + check.message, check.recommendation));
    }
}

void Logger::logSessionStart(const std::string& command, const std::string& user_prompt) {
    std::ostringstream context;
    context << "Command: " << command << ", ";
    context << "Prompt length: " << user_prompt.length() << " chars";
    
    info("Session", "Session started", context.str());
    debug("Session", "User prompt: " + user_prompt);
}

void Logger::logSessionEnd(const std::string& command, int exit_code, long duration_ms) {
    std::ostringstream context;
    context << "Command: " << command << ", ";
    context << "Exit code: " << exit_code << ", ";
    context << "Duration: " << duration_ms << "ms";
    
    if (exit_code == 0) {
        info("Session", "Session completed successfully", context.str());
    } else {
        error("Session", "Session completed with errors", context.str());
    }
}

void Logger::flush() {
    if (m_current_log_file && m_current_log_file->is_open()) {
        m_current_log_file->flush();
    }
}

std::string Logger::getLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRIT";
        default: return "UNKNOWN";
    }
}

std::string Logger::getLevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "\033[90m";     // Dark gray
        case LogLevel::INFO: return "\033[36m";      // Cyan
        case LogLevel::WARNING: return "\033[33m";   // Yellow
        case LogLevel::ERROR: return "\033[31m";     // Red
        case LogLevel::CRITICAL: return "\033[91m";  // Bright red
        default: return "\033[0m";                   // Reset
    }
}

void Logger::logEntry(const LogEntry& entry) {
    if (!m_initialized) {
        // Initialize with defaults if not done yet
        initialize();
    }
    
    writeToConsole(entry);
    writeToFile(entry);
}

void Logger::writeToConsole(const LogEntry& entry) {
    if (!m_console_enabled || entry.level < m_console_level) {
        return;
    }
    
    std::string formatted = formatEntry(entry, true);
    std::cout << formatted << std::endl;
}

void Logger::writeToFile(const LogEntry& entry) {
    if (!m_current_log_file || entry.level < m_file_level) {
        return;
    }
    
    rotateLogsIfNeeded();
    
    std::string formatted = formatEntry(entry, false);
    *m_current_log_file << formatted << std::endl;
    m_current_log_size += formatted.length() + 1; // +1 for newline
    
    // Flush critical and error messages immediately
    if (entry.level >= LogLevel::ERROR) {
        m_current_log_file->flush();
    }
}

std::string Logger::formatEntry(const LogEntry& entry, bool include_color) {
    std::ostringstream formatted;
    
    // Timestamp
    formatted << formatTimestamp(entry.timestamp) << " ";
    
    // Level with color
    if (include_color) {
        formatted << getLevelColor(entry.level);
    }
    formatted << "[" << getLevelName(entry.level) << "]";
    if (include_color) {
        formatted << "\033[0m"; // Reset color
    }
    formatted << " ";
    
    // Component
    formatted << entry.component << ": ";
    
    // Message
    formatted << entry.message;
    
    // Context (if provided)
    if (!entry.context.empty()) {
        formatted << " (" << entry.context << ")";
    }
    
    return formatted.str();
}

void Logger::rotateLogsIfNeeded() {
    if (m_current_log_size >= m_max_log_size) {
        // Close current file
        m_current_log_file.reset();
        
        // Create new file
        m_current_log_filename = generateLogFilename();
        m_current_log_file = std::make_unique<std::ofstream>(m_current_log_filename);
        m_current_log_size = 0;
        
        // Clean up old log files
        try {
            std::vector<std::filesystem::path> log_files;
            for (const auto& entry : std::filesystem::directory_iterator(m_log_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".log") {
                    log_files.push_back(entry.path());
                }
            }
            
            // Sort by modification time (newest first)
            std::sort(log_files.begin(), log_files.end(),
                     [](const std::filesystem::path& a, const std::filesystem::path& b) {
                         return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
                     });
            
            // Remove old files beyond the limit
            for (size_t i = m_max_log_files; i < log_files.size(); i++) {
                std::filesystem::remove(log_files[i]);
            }
            
        } catch (const std::exception& e) {
            // Log rotation failure shouldn't stop the program
            std::cerr << "[WARN] Log rotation failed: " << e.what() << std::endl;
        }
    }
}

std::string Logger::formatTimestamp(const std::chrono::system_clock::time_point& time_point) {
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        time_point.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

void Logger::ensureLogDirectory() {
    try {
        std::filesystem::create_directories(m_log_dir);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Cannot create log directory: " << e.what() << std::endl;
        // Fall back to current directory
        m_log_dir = ".";
    }
}

std::string Logger::generateLogFilename() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream filename;
    filename << m_log_dir << "/camus_";
    filename << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    filename << ".log";
    
    return filename.str();
}

} // namespace Camus