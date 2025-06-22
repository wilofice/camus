// =================================================================
// include/Camus/InteractiveConfirmation.hpp
// =================================================================
// Header for enhanced interactive confirmation interface.

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include "ResponseParser.hpp"

namespace Camus {

/**
 * @brief User action choice for file modifications
 */
enum class ConfirmationAction {
    ACCEPT,          ///< Accept this change
    REJECT,          ///< Reject this change
    ACCEPT_ALL,      ///< Accept all remaining changes
    REJECT_ALL,      ///< Reject all remaining changes
    VIEW_DIFF,       ///< View diff again
    VIEW_FULL,       ///< View full file content
    EDIT,            ///< Edit the modification (future feature)
    HELP,            ///< Show help
    QUIT             ///< Quit without applying any changes
};

/**
 * @brief Result of interactive confirmation session
 */
struct ConfirmationResult {
    std::vector<FileModification> accepted_modifications;
    std::vector<FileModification> rejected_modifications;
    bool session_completed;      ///< True if user completed review (not quit)
    size_t total_reviewed;       ///< Number of files reviewed
    
    ConfirmationResult() : session_completed(false), total_reviewed(0) {}
};

/**
 * @brief Options for interactive confirmation
 */
struct InteractiveOptions {
    bool show_preview;           ///< Show file preview before diff
    bool allow_editing;          ///< Allow inline editing of changes
    bool show_context_menu;      ///< Show context-sensitive menu
    bool auto_backup;            ///< Automatically create backups
    size_t preview_lines;        ///< Number of preview lines to show
    
    InteractiveOptions() : show_preview(true), allow_editing(false),
                          show_context_menu(true), auto_backup(true),
                          preview_lines(10) {}
};

/**
 * @brief Enhanced interactive confirmation interface for file modifications
 * 
 * Provides a rich, user-friendly interface for reviewing and confirming
 * file modifications with advanced options and navigation.
 */
class InteractiveConfirmation {
public:
    /**
     * @brief Construct a new InteractiveConfirmation
     */
    InteractiveConfirmation();

    /**
     * @brief Run interactive confirmation session
     * @param modifications Vector of file modifications to review
     * @param options Interactive options
     * @return Confirmation result with accepted/rejected files
     */
    ConfirmationResult runInteractiveSession(
        const std::vector<FileModification>& modifications,
        const InteractiveOptions& options = InteractiveOptions());

    /**
     * @brief Set custom action handler
     * @param action Action type
     * @param handler Function to handle the action
     */
    void setActionHandler(ConfirmationAction action,
                         std::function<void()> handler);

    /**
     * @brief Enable or disable specific actions
     * @param action Action to enable/disable
     * @param enabled True to enable
     */
    void setActionEnabled(ConfirmationAction action, bool enabled);

    /**
     * @brief Set custom prompt for action
     * @param prompt Custom prompt text
     */
    void setPrompt(const std::string& prompt);

private:
    std::string m_prompt;
    std::unordered_map<ConfirmationAction, bool> m_enabled_actions;
    std::unordered_map<ConfirmationAction, std::function<void()>> m_custom_handlers;
    std::unordered_map<std::string, ConfirmationAction> m_key_bindings;

    /**
     * @brief Initialize default key bindings
     */
    void initializeKeyBindings();

    /**
     * @brief Display interactive menu
     * @param current_file Current file being reviewed
     * @param file_index Current file index
     * @param total_files Total number of files
     * @param options Interactive options
     */
    void displayMenu(const FileModification& current_file,
                    size_t file_index, size_t total_files,
                    const InteractiveOptions& options) const;

    /**
     * @brief Get user action choice
     * @return Selected action
     */
    ConfirmationAction getUserAction() const;

    /**
     * @brief Display help information
     */
    void displayHelp() const;

    /**
     * @brief Display file preview
     * @param modification File modification to preview
     * @param num_lines Number of lines to show
     */
    void displayFilePreview(const FileModification& modification,
                           size_t num_lines) const;

    /**
     * @brief Display full file content with syntax highlighting
     * @param modification File modification to display
     */
    void displayFullFile(const FileModification& modification) const;

    /**
     * @brief Get file type for syntax highlighting
     * @param file_path File path
     * @return File type identifier
     */
    std::string getFileType(const std::string& file_path) const;

    /**
     * @brief Apply syntax highlighting to content
     * @param content File content
     * @param file_type Type of file
     * @return Highlighted content
     */
    std::string applySyntaxHighlighting(const std::string& content,
                                       const std::string& file_type) const;

    /**
     * @brief Clear screen for better UI
     */
    void clearScreen() const;

    /**
     * @brief Display progress indicator
     * @param current Current item
     * @param total Total items
     */
    void displayProgress(size_t current, size_t total) const;

    /**
     * @brief Format action description
     * @param action Action type
     * @return Human-readable description
     */
    std::string formatActionDescription(ConfirmationAction action) const;

    /**
     * @brief Check if action is enabled
     * @param action Action to check
     * @return True if enabled
     */
    bool isActionEnabled(ConfirmationAction action) const;

    /**
     * @brief Parse user input to action
     * @param input User input string
     * @return Corresponding action
     */
    ConfirmationAction parseUserInput(const std::string& input) const;

    /**
     * @brief Save session state for resume capability
     * @param result Current session result
     * @param current_index Current file index
     */
    void saveSessionState(const ConfirmationResult& result,
                         size_t current_index) const;

    /**
     * @brief Load previous session state if exists
     * @return True if session restored
     */
    bool loadSessionState(ConfirmationResult& result,
                         size_t& resume_index);
};

} // namespace Camus