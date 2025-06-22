// =================================================================
// src/Camus/Core.cpp
// =================================================================
// Implementation for the core application logic.

#include "Camus/Core.hpp"
#include "Camus/ConfigParser.hpp"
#include "Camus/LlmInteraction.hpp"
#include "Camus/LlamaCppInteraction.hpp"
#include "Camus/OllamaInteraction.hpp"
#include "Camus/SysInteraction.hpp"
#include "Camus/ProjectScanner.hpp"
#include "Camus/ContextBuilder.hpp"
#include "Camus/ResponseParser.hpp"
#include "Camus/MultiFileDiff.hpp"
#include "Camus/InteractiveConfirmation.hpp"
#include "Camus/BackupManager.hpp"
#include "dtl/dtl.hpp" // Include the new diff library header
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace Camus {

// Helper function to convert a string to a vector of lines
static std::vector<std::string> to_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    std::istringstream stream(text);
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

Core::Core(const Commands& commands) 
    : m_commands(commands),
      m_config(std::make_unique<ConfigParser>(".camus/config.yml")),
      m_sys(std::make_unique<SysInteraction>())
{
    // Only initialize the LLM for commands that need it.
    if (m_commands.active_command != "init" && !m_commands.active_command.empty()) {
        // Read the backend configuration
        std::string backend = m_config->getStringValue("backend");
        if (backend.empty()) {
            backend = "direct"; // Default to direct if not specified
        }
        
        try {
            if (backend == "ollama") {
                // Ollama backend configuration
                std::string ollama_url = m_config->getStringValue("ollama_url");
                std::string model_name = m_config->getStringValue("default_model");
                
                if (ollama_url.empty() || model_name.empty()) {
                    std::cerr << "[FATAL] `ollama_url` or `default_model` not set in .camus/config.yml" << std::endl;
                    std::cerr << "Please run 'camus init' and edit the configuration file." << std::endl;
                    m_llm = nullptr;
                    return;
                }
                
                std::cout << "[INFO] Using Ollama backend" << std::endl;
                m_llm = std::make_unique<OllamaInteraction>(ollama_url, model_name);
                
            } else {
                // Direct backend configuration (llama.cpp)
                std::string model_dir = m_config->getStringValue("model_path");
                std::string model_name = m_config->getStringValue("default_model");

                if (model_dir.empty() || model_name.empty()) {
                    std::cerr << "[FATAL] `model_path` or `default_model` not set in .camus/config.yml" << std::endl;
                    std::cerr << "Please run 'camus init' and edit the configuration file." << std::endl;
                    m_llm = nullptr;
                    return;
                }

                if (!model_dir.empty() && model_dir.back() != '/' && model_dir.back() != '\\') {
                    model_dir += '/';
                }
                
                std::string full_model_path = model_dir + model_name;
                
                std::cout << "[INFO] Using direct backend (llama.cpp)" << std::endl;
                m_llm = std::make_unique<LlamaCppInteraction>(full_model_path);
            }
        } catch (const std::exception& e) {
            std::cerr << "[FATAL] Failed to initialize LLM backend: " << backend << "\n"
                      << "Error: " << e.what() << std::endl;
            m_llm = nullptr;
        }
    }
}

Core::~Core() = default;

int Core::run() {
    if (m_commands.active_command != "init" && !m_commands.active_command.empty() && m_llm == nullptr) {
        std::cerr << "LLM not available. Cannot proceed." << std::endl;
        return 1;
    }
    
    if (m_commands.active_command == "init") {
        return handleInit();
    } else if (m_commands.active_command == "modify") {
        return handleModify();
    } else if (m_commands.active_command == "amodify") {
        return handleAmodify();
    } else if (m_commands.active_command == "refactor") {
        return handleRefactor();
    } else if (m_commands.active_command == "build") {
        return handleBuild();
    } else if (m_commands.active_command == "test") {
        return handleTest();
    } else if (m_commands.active_command == "commit") {
        return handleCommit();
    } else if (m_commands.active_command == "push") {
        return handlePush();
    } else if (m_commands.active_command.empty()){
        return 0;
    }

    std::cerr << "Error: Unknown command '" << m_commands.active_command << "'." << std::endl;
    return 1;
}

int Core::handleInit() {
    std::cout << "Initializing Camus configuration..." << std::endl;
    
    const std::string defaultConfig = R"(# Camus Configuration v1.0
# Backend configuration: 'direct' for llama.cpp or 'ollama' for Ollama server
backend: direct

# Direct backend settings (when backend: direct)
model_path: /path/to/your/models/
default_model: Llama-3-8B-Instruct.Q4_K_M.gguf

# Ollama backend settings (when backend: ollama)
ollama_url: http://localhost:11434

# Build and test commands
build_command: 'cmake --build ./build'
test_command: 'ctest --test-dir ./build'
)";

    const std::string configDir = ".camus";
    const std::string configFile = configDir + "/config.yml";

    if (!m_sys->directoryExists(configDir)) {
        if (!m_sys->createDirectory(configDir)) {
            std::cerr << "Error: Failed to create configuration directory '" << configDir << "'." << std::endl;
            return 1;
        }
        std::cout << "Created configuration directory: " << configDir << std::endl;
    }

    if (m_sys->fileExists(configFile)) {
        std::cout << "Configuration file '" << configFile << "' already exists. Skipping." << std::endl;
    } else {
        if (m_sys->writeFile(configFile, defaultConfig)) {
            std::cout << "Created default configuration file: " << configFile << std::endl;
            std::cout << "\nIMPORTANT: Please edit " << configFile << " to set the correct `model_path`." << std::endl;
        } else {
            std::cerr << "Error: Failed to write configuration file '" << configFile << "'." << std::endl;
            return 1;
        }
    }
    return 0;
}

int Core::handleModify() {
    std::cout << "Reading file: " << m_commands.file_path << "..." << std::endl;
    
    std::string original_code;
    try {
        original_code = m_sys->readFile(m_commands.file_path);
    } catch (const std::exception& e) {
        std::cerr << "Error reading file: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Constructing prompt for LLM..." << std::endl;
    std::stringstream prompt_stream;
    prompt_stream << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
                  << "You are an expert programmer. Your task is to modify the provided code based on the user's request. "
                  << "Your response must contain ONLY the raw, complete source code for the modified file. "
                  << "Do not include any conversational text, explanations, or markdown formatting like ``` or ```language.\n\n"
                  << "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n"
                  << " " << m_commands.prompt << "\n\n"
                  << "Original Code:\n"
                  << "```\n"
                  << original_code << "\n"
                  << "```\n\n"
                  << "<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";

    std::string full_prompt = prompt_stream.str();
    
    std::cout << "Sending request to LLM... (this may take a moment)" << std::endl;
    std::string modified_code;
    try {
        modified_code = m_llm->getCompletion(full_prompt);
    } catch (const std::exception& e) {
        std::cerr << "\nError during model generation: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n--- Proposed Changes ---" << std::endl;
    
    // Use the dtl library API
    auto original_lines = to_lines(original_code);
    auto modified_lines = to_lines(modified_code);
    
    using elem = std::string;
    using sequence = std::vector<elem>;
    dtl::Diff<elem, sequence> differ(original_lines, modified_lines);
    differ.compose();
    
    const std::string COLOR_RESET = "\033[0m";
    const std::string COLOR_RED = "\033[31m";
    const std::string COLOR_GREEN = "\033[32m";

    for (const auto& ses_item : differ.getSes().getSequence()) {
        if (ses_item.second.type == dtl::SES_DELETE) {
            std::cout << COLOR_RED << "- " << ses_item.first << COLOR_RESET << std::endl;
        } else if (ses_item.second.type == dtl::SES_ADD) {
            std::cout << COLOR_GREEN << "+ " << ses_item.first << COLOR_RESET << std::endl;
        } else {
            std::cout << "  " << ses_item.first << std::endl;
        }
    }
    std::cout << "------------------------" << std::endl;

    char confirmation;
    std::cout << "Apply changes? [y/n]: ";
    std::cin >> confirmation;

    if (confirmation == 'y' || confirmation == 'Y') {
        if (m_sys->writeFile(m_commands.file_path, modified_code)) {
            std::cout << "Changes applied successfully." << std::endl;
        } else {
            std::cerr << "Error: Failed to write changes to file." << std::endl;
            return 1;
        }
    } else {
        std::cout << "Changes discarded." << std::endl;
    }
    
    return 0;
}

int Core::handleAmodify() {
    std::cout << "Starting project-wide modification..." << std::endl;
    std::cout << "Request: " << m_commands.prompt << std::endl << std::endl;
    
    // Step 1: Scan project files
    std::cout << "[1/6] Scanning project files..." << std::endl;
    ProjectScanner scanner(".");
    
    // Apply include/exclude patterns if provided
    if (!m_commands.include_pattern.empty()) {
        // For now, we'll use simple pattern matching
        // In a full implementation, this would parse glob patterns
        scanner.addIncludeExtension(m_commands.include_pattern);
    }
    if (!m_commands.exclude_pattern.empty()) {
        scanner.addIgnorePattern(m_commands.exclude_pattern);
    }
    
    // Set max file limit
    scanner.setMaxFileSize(100 * 1024); // 100KB per file limit
    
    auto discovered_files = scanner.scanFiles();
    
    if (discovered_files.empty()) {
        std::cerr << "No relevant files found in the project." << std::endl;
        return 1;
    }
    
    // Limit files if needed
    if (discovered_files.size() > m_commands.max_files) {
        std::cout << "[WARN] Found " << discovered_files.size() 
                  << " files, limiting to " << m_commands.max_files << std::endl;
        discovered_files.resize(m_commands.max_files);
    }
    
    // Step 2: Build context
    std::cout << "[2/6] Building context from " << discovered_files.size() << " files..." << std::endl;
    ContextBuilder context_builder(m_commands.max_tokens);
    
    // Extract keywords from the user request for relevance scoring
    std::vector<std::string> keywords;
    std::istringstream iss(m_commands.prompt);
    std::string word;
    while (iss >> word) {
        if (word.length() > 3) { // Only consider words longer than 3 chars
            keywords.push_back(word);
        }
    }
    context_builder.setRelevanceKeywords(keywords);
    
    std::string context = context_builder.buildContext(discovered_files, m_commands.prompt);
    
    auto build_stats = context_builder.getLastBuildStats();
    std::cout << "Context built with " << build_stats["files_included"] 
              << " files (~" << build_stats["tokens_used"] << " tokens)" << std::endl;
    
    if (build_stats["files_truncated"] > 0) {
        std::cout << "[WARN] " << build_stats["files_truncated"] 
                  << " files were truncated to fit token limit" << std::endl;
    }
    
    // Step 3: Send to LLM
    std::cout << "[3/6] Sending request to LLM... (this may take a while)" << std::endl;
    std::string llm_response;
    try {
        llm_response = m_llm->getCompletion(context);
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] LLM request failed: " << e.what() << std::endl;
        return 1;
    }
    
    // Step 4: Parse response
    std::cout << "[4/6] Parsing LLM response..." << std::endl;
    ResponseParser parser(".");
    parser.setStrictValidation(true);
    
    auto modifications = parser.parseResponse(llm_response);
    auto parse_stats = parser.getLastParseStats();
    
    if (modifications.empty()) {
        std::cerr << "[ERROR] No valid file modifications found in LLM response." << std::endl;
        if (!parse_stats.error_messages.empty()) {
            std::cerr << "Parsing errors:" << std::endl;
            for (const auto& error : parse_stats.error_messages) {
                std::cerr << "  - " << error << std::endl;
            }
        }
        return 1;
    }
    
    std::cout << "Parsed " << modifications.size() << " file modifications" << std::endl;
    
    // Step 5: Create backups
    std::cout << "[5/6] Creating backups..." << std::endl;
    BackupManager backup_manager;
    
    std::vector<std::string> files_to_backup;
    for (const auto& mod : modifications) {
        if (!mod.is_new_file) {
            files_to_backup.push_back(mod.file_path);
        }
    }
    
    size_t backups_created = backup_manager.createBackups(files_to_backup);
    if (backups_created < files_to_backup.size()) {
        std::cerr << "[WARN] Could not create all backups. Proceed with caution." << std::endl;
    }
    
    // Step 6: Show diffs and get confirmation
    std::cout << "[6/6] Review proposed changes..." << std::endl << std::endl;
    
    // Check if we should use interactive mode
    bool use_interactive = modifications.size() > 5; // Use interactive for many files
    
    if (use_interactive) {
        InteractiveConfirmation interactive;
        InteractiveOptions options;
        options.show_preview = true;
        options.auto_backup = false; // We already created backups
        
        auto result = interactive.runInteractiveSession(modifications, options);
        
        if (!result.session_completed) {
            std::cout << "\nModification cancelled by user." << std::endl;
            backup_manager.cleanupBackups();
            return 0;
        }
        
        // Apply only accepted modifications
        modifications = result.accepted_modifications;
        
        if (modifications.empty()) {
            std::cout << "\nNo modifications accepted." << std::endl;
            backup_manager.cleanupBackups();
            return 0;
        }
    } else {
        // Use standard diff display for small number of files
        MultiFileDiff diff_viewer(".");
        
        if (!diff_viewer.showDiffsAndConfirm(modifications)) {
            std::cout << "\nModification cancelled by user." << std::endl;
            backup_manager.cleanupBackups();
            return 0;
        }
    }
    
    // Apply modifications
    std::cout << "\nApplying modifications to " << modifications.size() << " files..." << std::endl;
    
    size_t applied = 0;
    for (const auto& mod : modifications) {
        try {
            if (mod.is_new_file) {
                // Ensure directory exists
                std::filesystem::path file_path(mod.file_path);
                std::filesystem::create_directories(file_path.parent_path());
            }
            
            if (m_sys->writeFile(mod.file_path, mod.new_content)) {
                std::cout << "  ✓ " << mod.file_path;
                if (mod.is_new_file) {
                    std::cout << " (created)";
                }
                std::cout << std::endl;
                applied++;
            } else {
                std::cerr << "  ✗ " << mod.file_path << " (write failed)" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "  ✗ " << mod.file_path << " (error: " << e.what() << ")" << std::endl;
        }
    }
    
    std::cout << "\nSuccessfully applied " << applied << " of " 
              << modifications.size() << " modifications." << std::endl;
    
    if (applied < modifications.size()) {
        std::cout << "\n[WARN] Some modifications failed. You can restore from backup if needed." << std::endl;
        std::cout << "Backup location: .camus/backups/" << std::endl;
    } else {
        // Clean up backups on success
        backup_manager.cleanupBackups();
    }
    
    return (applied == modifications.size()) ? 0 : 1;
}

int Core::handleRefactor() {
    std::cout << "Executing 'refactor' command (Not yet implemented)." << std::endl;
    return 0;
}

int Core::handleBuild() {
    std::cout << "Executing build command..." << std::endl;
    
    // Read build command from config
    std::string build_command = m_config->getStringValue("build_command");
    
    if (build_command.empty()) {
        std::cerr << "Error: 'build_command' not configured in .camus/config.yml" << std::endl;
        std::cerr << "Please run 'camus init' and edit the configuration file." << std::endl;
        return 1;
    }
    
    // Parse the build command into executable and arguments
    std::vector<std::string> command_parts;
    std::stringstream ss(build_command);
    std::string part;
    bool in_quotes = false;
    std::string current_part;
    
    for (char c : build_command) {
        if (c == '\'' || c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!current_part.empty()) {
                command_parts.push_back(current_part);
                current_part.clear();
            }
        } else {
            current_part += c;
        }
    }
    if (!current_part.empty()) {
        command_parts.push_back(current_part);
    }
    
    if (command_parts.empty()) {
        std::cerr << "Error: Invalid build command format" << std::endl;
        return 1;
    }
    
    // Extract command and arguments
    std::string cmd = command_parts[0];
    std::vector<std::string> args(command_parts.begin() + 1, command_parts.end());
    
    // Add any passthrough arguments from the user
    args.insert(args.end(), m_commands.passthrough_args.begin(), m_commands.passthrough_args.end());
    
    std::cout << "Running: " << cmd;
    for (const auto& arg : args) {
        std::cout << " " << arg;
    }
    std::cout << std::endl;
    
    // Execute the build command
    auto [output, exit_code] = m_sys->executeCommand(cmd, args);
    
    // Display the output
    std::cout << output << std::endl;
    
    if (exit_code == 0) {
        std::cout << "Build completed successfully!" << std::endl;
    } else {
        std::cerr << "Build failed with exit code " << exit_code << std::endl;
        
        // If build failed, use LLM to analyze the error
        std::cout << "\nAnalyzing build errors..." << std::endl;
        
        std::stringstream prompt_stream;
        prompt_stream << "The following build command failed. Analyze the error output and suggest a fix. "
                      << "Be specific about what caused the error and how to fix it.\n\n"
                      << "Build Command: " << build_command << "\n\n"
                      << "Error Output:\n"
                      << output;
        
        std::string analysis = m_llm->getCompletion(prompt_stream.str());
        
        std::cout << "\n--- Build Error Analysis ---" << std::endl;
        std::cout << analysis << std::endl;
        std::cout << "---------------------------" << std::endl;
        
        return 1;
    }
    
    return 0;
}

int Core::handleTest() {
    std::cout << "Executing test command..." << std::endl;
    
    // Read test command from config
    std::string test_command = m_config->getStringValue("test_command");
    
    if (test_command.empty()) {
        std::cerr << "Error: 'test_command' not configured in .camus/config.yml" << std::endl;
        std::cerr << "Please run 'camus init' and edit the configuration file." << std::endl;
        return 1;
    }
    
    // Parse the test command into executable and arguments
    std::vector<std::string> command_parts;
    std::stringstream ss(test_command);
    std::string part;
    bool in_quotes = false;
    std::string current_part;
    
    for (char c : test_command) {
        if (c == '\'' || c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!current_part.empty()) {
                command_parts.push_back(current_part);
                current_part.clear();
            }
        } else {
            current_part += c;
        }
    }
    if (!current_part.empty()) {
        command_parts.push_back(current_part);
    }
    
    if (command_parts.empty()) {
        std::cerr << "Error: Invalid test command format" << std::endl;
        return 1;
    }
    
    // Extract command and arguments
    std::string cmd = command_parts[0];
    std::vector<std::string> args(command_parts.begin() + 1, command_parts.end());
    
    // Add any passthrough arguments from the user
    args.insert(args.end(), m_commands.passthrough_args.begin(), m_commands.passthrough_args.end());
    
    std::cout << "Running: " << cmd;
    for (const auto& arg : args) {
        std::cout << " " << arg;
    }
    std::cout << std::endl;
    
    // Execute the test command
    auto [output, exit_code] = m_sys->executeCommand(cmd, args);
    
    // Display the output
    std::cout << output << std::endl;
    
    if (exit_code == 0) {
        std::cout << "All tests passed!" << std::endl;
    } else {
        std::cerr << "Tests failed with exit code " << exit_code << std::endl;
        
        // If tests failed, use LLM to analyze the failure
        std::cout << "\nAnalyzing test failures..." << std::endl;
        
        std::stringstream prompt_stream;
        prompt_stream << "The following test command failed. Analyze the test output and suggest fixes for the failing tests. "
                      << "Be specific about what tests failed, why they failed, and how to fix them.\n\n"
                      << "Test Command: " << test_command << "\n\n"
                      << "Test Output:\n"
                      << output;
        
        std::string analysis = m_llm->getCompletion(prompt_stream.str());
        
        std::cout << "\n--- Test Failure Analysis ---" << std::endl;
        std::cout << analysis << std::endl;
        std::cout << "-----------------------------" << std::endl;
        
        return 1;
    }
    
    return 0;
}

int Core::handleCommit() {
    std::cout << "Checking for staged changes..." << std::endl;
    
    // Execute git diff --staged to get the staged changes
    auto [diff_output, exit_code] = m_sys->executeCommand("git", {"diff", "--staged"});
    
    if (exit_code != 0) {
        std::cerr << "Error: Failed to execute git diff --staged. Make sure you're in a git repository." << std::endl;
        return 1;
    }
    
    // Check if there are any staged changes
    if (diff_output.empty()) {
        std::cout << "No changes staged to commit." << std::endl;
        return 0;
    }
    
    std::cout << "Found staged changes. Generating commit message..." << std::endl;
    
    // Construct prompt for the LLM
    std::stringstream prompt_stream;
    prompt_stream << "Generate a concise, conventional commit message for the following code changes. "
                  << "The message should be descriptive and follow best practices. "
                  << "Output only the commit message, no explanations or additional text.\n\n"
                  << "--- Git Diff ---\n"
                  << diff_output;
    
    std::string full_prompt = prompt_stream.str();
    
    // Get commit message from LLM
    std::string commit_message = m_llm->getCompletion(full_prompt);
    
    // Remove any leading/trailing whitespace
    commit_message.erase(0, commit_message.find_first_not_of(" \n\r\t"));
    commit_message.erase(commit_message.find_last_not_of(" \n\r\t") + 1);
    
    // Display the generated commit message
    std::cout << "\n--- Generated Commit Message ---" << std::endl;
    std::cout << commit_message << std::endl;
    std::cout << "-------------------------------\n" << std::endl;
    
    // Prompt user for confirmation
    std::cout << "Use this commit message? [y/n]: ";
    std::string response;
    std::getline(std::cin, response);
    
    // Convert response to lowercase
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);
    
    if (response == "y" || response == "yes") {
        // Execute git commit with the generated message
        auto [commit_output, commit_exit_code] = m_sys->executeCommand("git", {"commit", "-m", commit_message});
        
        if (commit_exit_code == 0) {
            std::cout << "\nCommit created successfully!" << std::endl;
            std::cout << commit_output << std::endl;
        } else {
            std::cerr << "\nError: Failed to create commit" << std::endl;
            std::cerr << commit_output << std::endl;
            return 1;
        }
    } else {
        std::cout << "\nCommit aborted." << std::endl;
    }
    
    return 0;
}

int Core::handlePush() {
    std::cout << "Executing 'push' command (Not yet implemented)." << std::endl;
    return 0;
}

} // namespace Camus
