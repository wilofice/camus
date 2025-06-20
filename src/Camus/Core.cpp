// =================================================================
// src/Camus/Core.cpp
// =================================================================
// Implementation for the core application logic.

#include "Camus/Core.hpp"
#include "Camus/ConfigParser.hpp"
#include "Camus/LlmInteraction.hpp"
#include "Camus/SysInteraction.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>

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
    prompt_stream << "You are an expert C++ programmer. Your task is to modify the following code based on the user's request. "
                  << "You must only output the complete, modified source code. Do not add any conversational text, explanations, or markdown code blocks.\n\n"
                  << "User Request: " << m_commands.prompt << "\n\n"
                  << "--- Original Code ---\n"
                  << file_content;

    std::string full_prompt = prompt_stream.str();
    
    std::cout << "Sending request to LLM... (this may take a moment)" << std::endl;
    std::string modified_code = m_llm->getCompletion(full_prompt);

    std::cout << "\n--- LLM Response Received ---" << std::endl;
    // TODO: Implement a proper diff view between 'file_content' and 'modified_code'.
    std::cout << modified_code << std::endl;
    std::cout << "---------------------------" << std::endl;

    // TODO: Implement interactive confirmation to write changes to file.
    
    return 0;
}

int Core::handleRefactor() {
    std::cout << "Executing 'refactor' command (Not yet implemented)." << std::endl;
    return 0;
}

int Core::handleBuild() {
    std::cout << "Executing 'build' command (Not yet implemented)." << std::endl;
    return 0;
}

int Core::handleTest() {
    std::cout << "Executing 'test' command (Not yet implemented)." << std::endl;
    return 0;
}

int Core::handleCommit() {
    std::cout << "Executing 'commit' command (Not yet implemented)." << std::endl;
    return 0;
}

int Core::handlePush() {
    std::cout << "Executing 'push' command (Not yet implemented)." << std::endl;
    return 0;
}

} // namespace Camus
