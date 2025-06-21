// =================================================================
// src/Camus/Core.cpp
// =================================================================
// Implementation for the core application logic.

#include "Camus/Core.hpp"
#include "Camus/ConfigParser.hpp"
#include "Camus/LlmInteraction.hpp"
#include "Camus/SysInteraction.hpp"
#include "dtl/dtl.hpp" // Include the new diff library header
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>

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
model_path: /path/to/your/models/
default_model: Llama-3-8B-Instruct.Q4_K_M.gguf
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
                  << "Modify the following code with this request: " << m_commands.prompt << "\n\n"
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
