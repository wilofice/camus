// =================================================================
// src/Camus/Core.cpp
// =================================================================
// Implementation for the core application logic.

#include "Camus/Core.hpp"
#include "Camus/ConfigParser.hpp"
#include "Camus/LlmInteraction.hpp"
#include "Camus/SysInteraction.hpp"
#include "Camus/DiffGenerator.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>

namespace Camus {

// The constructor now correctly uses the member initializer list for all members.
Core::Core(const Commands& commands) 
    : m_commands(commands),
      m_config(std::make_unique<ConfigParser>(".camus/config.yml")),
      m_sys(std::make_unique<SysInteraction>())
{
    // Only initialize the LLM for commands that need it.
    if (m_commands.active_command != "init" && !m_commands.active_command.empty()) {
        std::string model_dir = m_config->getStringValue("model_path");
        std::string model_name = m_config->getStringValue("default_model");

        if (model_dir.empty() || model_name.empty()) {
            std::cerr << "[FATAL] `model_path` or `default_model` not set in .camus/config.yml" << std::endl;
            std::cerr << "Please run 'camus init' and edit the configuration file." << std::endl;
            m_llm = nullptr;
            return;
        }

        // Ensure model_path has a trailing slash
        if (!model_dir.empty() && model_dir.back() != '/' && model_dir.back() != '\\') {
            model_dir += '/';
        }
        
        std::string full_model_path = model_dir + model_name;
        
        try {
           m_llm = std::make_unique<LlmInteraction>(full_model_path);
        } catch (const std::exception& e) {
            std::cerr << "[FATAL] Failed to initialize LLM with model: " << full_model_path << "\n"
                      << "Please check the path in .camus/config.yml and ensure the model exists.\n"
                      << "Error: " << e.what() << std::endl;
            m_llm = nullptr;
        }
    }
}

// Explicitly define the destructor here in the .cpp file.
// This ensures that the destructors for unique_ptr members are instantiated
// only after their full types are known from the includes above.
Core::~Core() = default;

int Core::run() {
    // Check if initialization failed for commands that need the LLM
    if (m_commands.active_command != "init" && !m_commands.active_command.empty() && m_llm == nullptr) {
        std::cerr << "LLM not available. Cannot proceed." << std::endl;
        return 1;
    }
    
    if (m_commands.active_command == "init") {
        return handleInit();
    } else if (m_commands.active_command == "modify") {
        return handleModify();
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
        return 0; // Success for showing help, etc.
    }

    std::cerr << "Error: Unknown command '" << m_commands.active_command << "'." << std::endl;
    return 1;
}

int Core::handleInit() {
    std::cout << "Initializing Camus configuration..." << std::endl;
    
    const std::string defaultConfig = R"(# Camus Configuration v1.0
# Path to the directory containing GGUF models.
model_path: /path/to/your/models/

# Default model to use for operations.
default_model: Llama-3-8B-Instruct.Q4_K_M.gguf

# Command to build the project.
build_command: 'cmake --build ./build'

# Command to run tests.
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
    
    std::string file_content;
    try {
        file_content = m_sys->readFile(m_commands.file_path);
    } catch (const std::exception& e) {
        std::cerr << "Error reading file: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Constructing prompt for LLM..." << std::endl;
    std::stringstream prompt_stream;
    prompt_stream << "You are an expert programmer in many languages. Your task is to modify the following code based on the user's request. "
                  << "You must only output the complete, modified source code. Do not add any conversational text, explanations, or markdown code blocks.\n\n"
                  << "User Request: " << m_commands.prompt << "\n\n"
                  << "--- Original Code ---\n"
                  << file_content;

    std::string full_prompt = prompt_stream.str();
    
    std::cout << "Sending request to LLM... (this may take a moment)" << std::endl;
    std::string modified_code = m_llm->getCompletion(full_prompt);

    std::cout << "\n--- LLM Response Received ---" << std::endl;
    
    // Create DiffGenerator and display colored diff
    DiffGenerator differ(file_content, modified_code);
    differ.printColoredDiff();
    
    // Prompt user for confirmation
    std::cout << "\nApply changes? [y]es, [n]o: ";
    std::string response;
    std::getline(std::cin, response);
    
    // Convert response to lowercase for easier comparison
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);
    
    if (response == "y" || response == "yes") {
        // Write the modified code to the file
        if (m_sys->writeFile(m_commands.file_path, modified_code)) {
            std::cout << "\nChanges applied successfully to " << m_commands.file_path << std::endl;
        } else {
            std::cerr << "\nError: Failed to write changes to file." << std::endl;
            return 1;
        }
    } else {
        std::cout << "\nChanges discarded." << std::endl;
    }
    
    return 0;
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
        std::cerr << "Error: Failed to execute git diff --staged" << std::endl;
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
    std::cout << "Use this commit message? [y]es, [e]dit, [n]o: ";
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
    } else if (response == "e" || response == "edit") {
        // Allow user to edit the message
        std::cout << "\nEnter your commit message (press Enter twice to finish):\n";
        std::string user_message;
        std::string line;
        int empty_lines = 0;
        
        while (empty_lines < 2) {
            std::getline(std::cin, line);
            if (line.empty()) {
                empty_lines++;
                if (empty_lines < 2 && !user_message.empty()) {
                    user_message += "\n";
                }
            } else {
                empty_lines = 0;
                if (!user_message.empty()) {
                    user_message += "\n";
                }
                user_message += line;
            }
        }
        
        // Remove trailing newlines
        while (!user_message.empty() && user_message.back() == '\n') {
            user_message.pop_back();
        }
        
        if (!user_message.empty()) {
            auto [commit_output, commit_exit_code] = m_sys->executeCommand("git", {"commit", "-m", user_message});
            
            if (commit_exit_code == 0) {
                std::cout << "\nCommit created successfully!" << std::endl;
                std::cout << commit_output << std::endl;
            } else {
                std::cerr << "\nError: Failed to create commit" << std::endl;
                std::cerr << commit_output << std::endl;
                return 1;
            }
        } else {
            std::cout << "\nNo commit message provided. Aborting commit." << std::endl;
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
