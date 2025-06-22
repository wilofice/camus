// =================================================================
// include/Camus/Logger.hpp
// =================================================================
// Header for comprehensive logging and audit trails.

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <memory>

namespace Camus {

/**
 * @brief Log levels for message classification
 */
enum class LogLevel {
    DEBUG,      ///< Detailed debug information
    INFO,       ///< General information
    WARNING,    ///< Warning conditions
    ERROR,      ///< Error conditions
    CRITICAL    ///< Critical conditions
};

/**
 * @brief Log entry structure
 */
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string component;
    std::string message;
    std::string context;
    
    LogEntry(LogLevel lvl, const std::string& comp, const std::string& msg, const std::string& ctx = "")
        : timestamp(std::chrono::system_clock::now()), level(lvl), component(comp), message(msg), context(ctx) {}
};

/**
 * @brief Comprehensive logging system for debugging and audit trails
 * 
 * Provides structured logging with multiple output targets, log levels,
 * and specialized logging for different components of the system.
 */
class Logger {
public:
    /**
     * @brief Get the singleton logger instance
     * @return Reference to logger instance
     */
    static Logger& getInstance();

    /**
     * @brief Initialize logger with configuration
     * @param log_dir Directory for log files
     * @param max_log_size Maximum size per log file (bytes)
     * @param max_log_files Maximum number of log files to keep
     */
    void initialize(const std::string& log_dir = ".camus/logs",
                   size_t max_log_size = 10 * 1024 * 1024,  // 10MB
                   size_t max_log_files = 5);

    /**
     * @brief Set minimum log level for console output
     * @param level Minimum level to display on console
     */
    void setConsoleLogLevel(LogLevel level);

    /**
     * @brief Set minimum log level for file output
     * @param level Minimum level to write to files
     */
    void setFileLogLevel(LogLevel level);

    /**
     * @brief Enable or disable console logging
     * @param enabled True to enable console output
     */
    void setConsoleLogging(bool enabled);

    /**
     * @brief Log a debug message
     * @param component Component name
     * @param message Log message
     * @param context Additional context
     */
    void debug(const std::string& component, const std::string& message, const std::string& context = "");

    /**
     * @brief Log an info message
     * @param component Component name
     * @param message Log message
     * @param context Additional context
     */
    void info(const std::string& component, const std::string& message, const std::string& context = "");

    /**
     * @brief Log a warning message
     * @param component Component name
     * @param message Log message
     * @param context Additional context
     */
    void warning(const std::string& component, const std::string& message, const std::string& context = "");

    /**
     * @brief Log an error message
     * @param component Component name
     * @param message Log message
     * @param context Additional context
     */
    void error(const std::string& component, const std::string& message, const std::string& context = "");

    /**
     * @brief Log a critical message
     * @param component Component name
     * @param message Log message
     * @param context Additional context
     */
    void critical(const std::string& component, const std::string& message, const std::string& context = "");

    /**
     * @brief Log file discovery results
     * @param discovered_files List of discovered files
     * @param filtered_files List of filtered files
     */
    void logFileDiscovery(const std::vector<std::string>& discovered_files,
                         const std::vector<std::string>& filtered_files);

    /**
     * @brief Log context building statistics
     * @param total_files Total files processed
     * @param included_files Files included in context
     * @param tokens_used Estimated tokens used
     * @param truncated_files Number of truncated files
     */
    void logContextBuilding(size_t total_files, size_t included_files,
                           size_t tokens_used, size_t truncated_files);

    /**
     * @brief Log LLM request/response metadata
     * @param request_tokens Estimated request tokens
     * @param response_size Response size in characters
     * @param duration_ms Request duration in milliseconds
     * @param success Whether request succeeded
     */
    void logLlmInteraction(size_t request_tokens, size_t response_size,
                          long duration_ms, bool success);

    /**
     * @brief Log file modification operations
     * @param modifications List of file modifications
     * @param applied_count Number successfully applied
     * @param failed_count Number that failed
     */
    void logFileModifications(const std::vector<struct FileModification>& modifications,
                             size_t applied_count, size_t failed_count);

    /**
     * @brief Log safety check results
     * @param safety_report Safety check report
     */
    void logSafetyChecks(const struct SafetyReport& safety_report);

    /**
     * @brief Log session start
     * @param command Command being executed
     * @param user_prompt User prompt/request
     */
    void logSessionStart(const std::string& command, const std::string& user_prompt);

    /**
     * @brief Log session end
     * @param command Command that was executed
     * @param exit_code Exit code
     * @param duration_ms Session duration in milliseconds
     */
    void logSessionEnd(const std::string& command, int exit_code, long duration_ms);

    /**
     * @brief Flush all log buffers
     */
    void flush();

    /**
     * @brief Get log level name as string
     * @param level Log level
     * @return String representation
     */
    static std::string getLevelName(LogLevel level);

    /**
     * @brief Get log level color for console output
     * @param level Log level
     * @return ANSI color code
     */
    static std::string getLevelColor(LogLevel level);

private:
    Logger() = default;
    ~Logger();
    
    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string m_log_dir;
    size_t m_max_log_size;
    size_t m_max_log_files;
    LogLevel m_console_level;
    LogLevel m_file_level;
    bool m_console_enabled;
    bool m_initialized;
    
    std::unique_ptr<std::ofstream> m_current_log_file;
    std::string m_current_log_filename;
    size_t m_current_log_size;

    /**
     * @brief Log an entry to all configured outputs
     * @param entry Log entry to write
     */
    void logEntry(const LogEntry& entry);

    /**
     * @brief Write entry to console if appropriate
     * @param entry Log entry
     */
    void writeToConsole(const LogEntry& entry);

    /**
     * @brief Write entry to file if appropriate
     * @param entry Log entry
     */
    void writeToFile(const LogEntry& entry);

    /**
     * @brief Format log entry for output
     * @param entry Log entry
     * @param include_color Whether to include color codes
     * @return Formatted string
     */
    std::string formatEntry(const LogEntry& entry, bool include_color = false);

    /**
     * @brief Rotate log files if needed
     */
    void rotateLogsIfNeeded();

    /**
     * @brief Generate timestamp string
     * @param time_point Time to format
     * @return Formatted timestamp
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time_point);

    /**
     * @brief Ensure log directory exists
     */
    void ensureLogDirectory();

    /**
     * @brief Get new log filename
     * @return Filename for new log file
     */
    std::string generateLogFilename();
};

// Convenience macros for logging
#define LOG_DEBUG(component, message) \
    Camus::Logger::getInstance().debug(component, message)

#define LOG_INFO(component, message) \
    Camus::Logger::getInstance().info(component, message)

#define LOG_WARNING(component, message) \
    Camus::Logger::getInstance().warning(component, message)

#define LOG_ERROR(component, message) \
    Camus::Logger::getInstance().error(component, message)

#define LOG_CRITICAL(component, message) \
    Camus::Logger::getInstance().critical(component, message)

} // namespace Camus