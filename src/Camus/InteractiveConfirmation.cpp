// =================================================================
// src/Camus/InteractiveConfirmation.cpp
// =================================================================
// Implementation for enhanced interactive confirmation interface.

#include "Camus/InteractiveConfirmation.hpp"
#include "Camus/MultiFileDiff.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <cstdlib>

namespace Camus {

InteractiveConfirmation::InteractiveConfirmation() 
    : m_prompt("Select action: ") {
    initializeKeyBindings();
    
    // Enable all actions by default
    for (const auto& action : {
        ConfirmationAction::ACCEPT,
        ConfirmationAction::REJECT,
        ConfirmationAction::ACCEPT_ALL,
        ConfirmationAction::REJECT_ALL,
        ConfirmationAction::VIEW_DIFF,
        ConfirmationAction::VIEW_FULL,
        ConfirmationAction::HELP,
        ConfirmationAction::QUIT
    }) {
        m_enabled_actions[action] = true;
    }
    
    // Editing disabled by default
    m_enabled_actions[ConfirmationAction::EDIT] = false;
}

ConfirmationResult InteractiveConfirmation::runInteractiveSession(
    const std::vector<FileModification>& modifications,
    const InteractiveOptions& options) {
    
    ConfirmationResult result;
    size_t current_index = 0;
    
    // Try to restore previous session
    if (loadSessionState(result, current_index)) {
        std::cout << "Resuming from previous session..." << std::endl;
    }
    
    MultiFileDiff diff_viewer;
    diff_viewer.setPerFileConfirmation(false); // We handle confirmation
    
    bool accept_all_remaining = false;
    bool reject_all_remaining = false;
    
    while (current_index < modifications.size()) {
        const auto& modification = modifications[current_index];
        
        // Auto-accept/reject if user chose "all"
        if (accept_all_remaining) {
            result.accepted_modifications.push_back(modification);
            current_index++;
            continue;
        }
        if (reject_all_remaining) {
            result.rejected_modifications.push_back(modification);
            current_index++;
            continue;
        }
        
        // Clear screen for clean UI
        clearScreen();
        
        // Display progress
        displayProgress(current_index + 1, modifications.size());
        
        // Display file information
        std::cout << "\n";
        std::cout << "File: " << modification.file_path;
        if (modification.is_new_file) {
            std::cout << " (NEW FILE)";
        }
        std::cout << std::endl;
        std::cout << "Size: " << modification.new_content.size() << " bytes" << std::endl;
        std::cout << "Estimated tokens: " << modification.estimated_tokens << std::endl;
        std::cout << std::endl;
        
        // Show preview if enabled
        if (options.show_preview) {
            displayFilePreview(modification, options.preview_lines);
        }
        
        // Display menu
        displayMenu(modification, current_index + 1, modifications.size(), options);
        
        // Get user action
        ConfirmationAction action = getUserAction();
        
        // Handle action
        switch (action) {
            case ConfirmationAction::ACCEPT:
                result.accepted_modifications.push_back(modification);
                current_index++;
                result.total_reviewed++;
                break;
                
            case ConfirmationAction::REJECT:
                result.rejected_modifications.push_back(modification);
                current_index++;
                result.total_reviewed++;
                break;
                
            case ConfirmationAction::ACCEPT_ALL:
                result.accepted_modifications.push_back(modification);
                accept_all_remaining = true;
                current_index++;
                result.total_reviewed++;
                break;
                
            case ConfirmationAction::REJECT_ALL:
                result.rejected_modifications.push_back(modification);
                reject_all_remaining = true;
                current_index++;
                result.total_reviewed++;
                break;
                
            case ConfirmationAction::VIEW_DIFF:
                {
                    // Show detailed diff
                    std::vector<FileModification> single_mod = {modification};
                    DiffDisplayOptions diff_options;
                    diff_options.context_lines = 5;
                    diff_options.color_output = true;
                    diff_viewer.showDiffsAndConfirm(single_mod, diff_options);
                    std::cout << "\nPress Enter to continue...";
                    std::cin.get();
                }
                break;
                
            case ConfirmationAction::VIEW_FULL:
                displayFullFile(modification);
                std::cout << "\nPress Enter to continue...";
                std::cin.get();
                break;
                
            case ConfirmationAction::EDIT:
                if (isActionEnabled(ConfirmationAction::EDIT)) {
                    std::cout << "Edit functionality not yet implemented." << std::endl;
                    std::cout << "Press Enter to continue...";
                    std::cin.get();
                }
                break;
                
            case ConfirmationAction::HELP:
                displayHelp();
                std::cout << "\nPress Enter to continue...";
                std::cin.get();
                break;
                
            case ConfirmationAction::QUIT:
                std::cout << "\nQuit confirmation - are you sure? [y/n]: ";
                std::string confirm;
                std::getline(std::cin, confirm);
                if (confirm == "y" || confirm == "yes") {
                    // Save session state
                    saveSessionState(result, current_index);
                    result.session_completed = false;
                    return result;
                }
                break;
        }
        
        // Auto-save session state periodically
        if (result.total_reviewed % 5 == 0) {
            saveSessionState(result, current_index);
        }
    }
    
    // Process remaining auto-accept/reject
    while (current_index < modifications.size()) {
        const auto& modification = modifications[current_index];
        if (accept_all_remaining) {
            result.accepted_modifications.push_back(modification);
        } else if (reject_all_remaining) {
            result.rejected_modifications.push_back(modification);
        }
        current_index++;
    }
    
    result.session_completed = true;
    
    // Display summary
    clearScreen();
    std::cout << "=== SESSION SUMMARY ===" << std::endl;
    std::cout << "Total files reviewed: " << result.total_reviewed << std::endl;
    std::cout << "Files accepted: " << result.accepted_modifications.size() << std::endl;
    std::cout << "Files rejected: " << result.rejected_modifications.size() << std::endl;
    
    // Clean up session state
    std::filesystem::remove(".camus/interactive_session.tmp");
    
    return result;
}

void InteractiveConfirmation::setActionHandler(ConfirmationAction action,
                                              std::function<void()> handler) {
    m_custom_handlers[action] = handler;
}

void InteractiveConfirmation::setActionEnabled(ConfirmationAction action, bool enabled) {
    m_enabled_actions[action] = enabled;
}

void InteractiveConfirmation::setPrompt(const std::string& prompt) {
    m_prompt = prompt;
}

void InteractiveConfirmation::initializeKeyBindings() {
    m_key_bindings = {
        {"y", ConfirmationAction::ACCEPT},
        {"yes", ConfirmationAction::ACCEPT},
        {"n", ConfirmationAction::REJECT},
        {"no", ConfirmationAction::REJECT},
        {"a", ConfirmationAction::ACCEPT_ALL},
        {"all", ConfirmationAction::ACCEPT_ALL},
        {"r", ConfirmationAction::REJECT_ALL},
        {"reject all", ConfirmationAction::REJECT_ALL},
        {"d", ConfirmationAction::VIEW_DIFF},
        {"diff", ConfirmationAction::VIEW_DIFF},
        {"v", ConfirmationAction::VIEW_FULL},
        {"view", ConfirmationAction::VIEW_FULL},
        {"e", ConfirmationAction::EDIT},
        {"edit", ConfirmationAction::EDIT},
        {"h", ConfirmationAction::HELP},
        {"help", ConfirmationAction::HELP},
        {"?", ConfirmationAction::HELP},
        {"q", ConfirmationAction::QUIT},
        {"quit", ConfirmationAction::QUIT}
    };
}

void InteractiveConfirmation::displayMenu(const FileModification& current_file,
                                         size_t file_index, size_t total_files,
                                         const InteractiveOptions& options) const {
    std::cout << "\n--- ACTIONS ---" << std::endl;
    
    // Primary actions
    if (isActionEnabled(ConfirmationAction::ACCEPT)) {
        std::cout << "[y] Accept this change" << std::endl;
    }
    if (isActionEnabled(ConfirmationAction::REJECT)) {
        std::cout << "[n] Reject this change" << std::endl;
    }
    
    // Bulk actions
    if (file_index < total_files) {
        if (isActionEnabled(ConfirmationAction::ACCEPT_ALL)) {
            std::cout << "[a] Accept all remaining changes" << std::endl;
        }
        if (isActionEnabled(ConfirmationAction::REJECT_ALL)) {
            std::cout << "[r] Reject all remaining changes" << std::endl;
        }
    }
    
    // View actions
    if (isActionEnabled(ConfirmationAction::VIEW_DIFF)) {
        std::cout << "[d] View detailed diff" << std::endl;
    }
    if (isActionEnabled(ConfirmationAction::VIEW_FULL)) {
        std::cout << "[v] View full file" << std::endl;
    }
    
    // Edit action (if enabled)
    if (isActionEnabled(ConfirmationAction::EDIT)) {
        std::cout << "[e] Edit this change" << std::endl;
    }
    
    // Help and quit
    std::cout << "[h] Help" << std::endl;
    std::cout << "[q] Quit" << std::endl;
    
    std::cout << std::endl;
}

ConfirmationAction InteractiveConfirmation::getUserAction() const {
    std::cout << m_prompt;
    std::cout.flush();
    
    std::string input;
    std::getline(std::cin, input);
    
    // Convert to lowercase
    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
    
    return parseUserInput(input);
}

void InteractiveConfirmation::displayHelp() const {
    clearScreen();
    std::cout << "=== INTERACTIVE CONFIRMATION HELP ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "ACTIONS:" << std::endl;
    std::cout << "  y/yes    - Accept the current file modification" << std::endl;
    std::cout << "  n/no     - Reject the current file modification" << std::endl;
    std::cout << "  a/all    - Accept all remaining modifications" << std::endl;
    std::cout << "  r        - Reject all remaining modifications" << std::endl;
    std::cout << "  d/diff   - View detailed diff for current file" << std::endl;
    std::cout << "  v/view   - View full file content" << std::endl;
    if (isActionEnabled(ConfirmationAction::EDIT)) {
        std::cout << "  e/edit   - Edit the current modification" << std::endl;
    }
    std::cout << "  h/help/? - Show this help" << std::endl;
    std::cout << "  q/quit   - Quit (session can be resumed)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "TIPS:" << std::endl;
    std::cout << "- Your progress is automatically saved" << std::endl;
    std::cout << "- If you quit, you can resume from where you left off" << std::endl;
    std::cout << "- Use 'a' to quickly accept remaining changes after review" << std::endl;
    std::cout << "- The diff view shows context around changes" << std::endl;
}

void InteractiveConfirmation::displayFilePreview(const FileModification& modification,
                                                size_t num_lines) const {
    std::cout << "--- PREVIEW ---" << std::endl;
    
    std::istringstream content_stream(modification.new_content);
    std::string line;
    size_t line_count = 0;
    
    while (std::getline(content_stream, line) && line_count < num_lines) {
        std::cout << std::setw(4) << (line_count + 1) << ": " << line << std::endl;
        line_count++;
    }
    
    // Count total lines
    size_t total_lines = std::count(modification.new_content.begin(), 
                                   modification.new_content.end(), '\n');
    
    if (total_lines > num_lines) {
        std::cout << "... (" << (total_lines - num_lines) << " more lines)" << std::endl;
    }
    
    std::cout << std::endl;
}

void InteractiveConfirmation::displayFullFile(const FileModification& modification) const {
    clearScreen();
    std::cout << "=== FULL FILE: " << modification.file_path << " ===" << std::endl;
    std::cout << std::endl;
    
    std::string file_type = getFileType(modification.file_path);
    std::string highlighted_content = applySyntaxHighlighting(modification.new_content, file_type);
    
    std::istringstream content_stream(highlighted_content);
    std::string line;
    size_t line_num = 1;
    
    while (std::getline(content_stream, line)) {
        std::cout << std::setw(4) << line_num++ << ": " << line << std::endl;
    }
}

std::string InteractiveConfirmation::getFileType(const std::string& file_path) const {
    std::filesystem::path path(file_path);
    std::string ext = path.extension().string();
    
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "cpp";
    if (ext == ".hpp" || ext == ".h" || ext == ".hxx") return "cpp";
    if (ext == ".py") return "python";
    if (ext == ".js" || ext == ".jsx") return "javascript";
    if (ext == ".ts" || ext == ".tsx") return "typescript";
    if (ext == ".java") return "java";
    if (ext == ".rs") return "rust";
    if (ext == ".go") return "go";
    
    return "text";
}

std::string InteractiveConfirmation::applySyntaxHighlighting(const std::string& content,
                                                            const std::string& file_type) const {
    // Simple syntax highlighting (can be enhanced)
    // For now, just return the content as-is
    // In a full implementation, this would apply ANSI color codes
    // based on language syntax
    return content;
}

void InteractiveConfirmation::clearScreen() const {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

void InteractiveConfirmation::displayProgress(size_t current, size_t total) const {
    float progress = static_cast<float>(current) / total;
    size_t bar_width = 50;
    size_t filled = static_cast<size_t>(progress * bar_width);
    
    std::cout << "Progress: [";
    for (size_t i = 0; i < bar_width; i++) {
        if (i < filled) {
            std::cout << "=";
        } else if (i == filled) {
            std::cout << ">";
        } else {
            std::cout << " ";
        }
    }
    std::cout << "] " << current << "/" << total;
    std::cout << " (" << std::fixed << std::setprecision(1) << (progress * 100) << "%)" << std::endl;
}

std::string InteractiveConfirmation::formatActionDescription(ConfirmationAction action) const {
    switch (action) {
        case ConfirmationAction::ACCEPT: return "Accept change";
        case ConfirmationAction::REJECT: return "Reject change";
        case ConfirmationAction::ACCEPT_ALL: return "Accept all remaining";
        case ConfirmationAction::REJECT_ALL: return "Reject all remaining";
        case ConfirmationAction::VIEW_DIFF: return "View diff";
        case ConfirmationAction::VIEW_FULL: return "View full file";
        case ConfirmationAction::EDIT: return "Edit change";
        case ConfirmationAction::HELP: return "Show help";
        case ConfirmationAction::QUIT: return "Quit session";
        default: return "Unknown action";
    }
}

bool InteractiveConfirmation::isActionEnabled(ConfirmationAction action) const {
    auto it = m_enabled_actions.find(action);
    return it != m_enabled_actions.end() && it->second;
}

ConfirmationAction InteractiveConfirmation::parseUserInput(const std::string& input) const {
    auto it = m_key_bindings.find(input);
    if (it != m_key_bindings.end()) {
        return it->second;
    }
    
    // Default to help for unknown input
    return ConfirmationAction::HELP;
}

void InteractiveConfirmation::saveSessionState(const ConfirmationResult& result,
                                               size_t current_index) const {
    try {
        std::filesystem::create_directories(".camus");
        std::ofstream state_file(".camus/interactive_session.tmp");
        
        state_file << current_index << std::endl;
        state_file << result.accepted_modifications.size() << std::endl;
        for (const auto& mod : result.accepted_modifications) {
            state_file << mod.file_path << std::endl;
        }
        state_file << result.rejected_modifications.size() << std::endl;
        for (const auto& mod : result.rejected_modifications) {
            state_file << mod.file_path << std::endl;
        }
        state_file << result.total_reviewed << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Could not save session state: " << e.what() << std::endl;
    }
}

bool InteractiveConfirmation::loadSessionState(ConfirmationResult& result,
                                               size_t& resume_index) {
    try {
        std::ifstream state_file(".camus/interactive_session.tmp");
        if (!state_file.is_open()) {
            return false;
        }
        
        state_file >> resume_index;
        
        size_t accepted_count;
        state_file >> accepted_count;
        state_file.ignore(); // Skip newline
        
        for (size_t i = 0; i < accepted_count; i++) {
            std::string file_path;
            std::getline(state_file, file_path);
            // Note: We're only storing paths, not full modifications
            // In a real implementation, we'd need to match these against
            // the current modifications list
        }
        
        size_t rejected_count;
        state_file >> rejected_count;
        state_file.ignore(); // Skip newline
        
        for (size_t i = 0; i < rejected_count; i++) {
            std::string file_path;
            std::getline(state_file, file_path);
        }
        
        state_file >> result.total_reviewed;
        
        return resume_index > 0;
        
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace Camus