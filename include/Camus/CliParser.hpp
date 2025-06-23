// =================================================================
// include/Camus/CliParser.hpp
// =================================================================
// Defines the interface for parsing command-line arguments.
// This class encapsulates all interaction with the CLI11 library.

#pragma once

#include "CLI/CLI.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Camus {

// A simple struct to hold parsed command information.
struct Commands {
    std::string active_command; // Name of the subcommand triggered

    // Options for 'modify' and 'refactor'
    std::string prompt;
    std::string file_path;

    // Options for 'amodify' (project-wide modifications)
    size_t max_files = 100;
    size_t max_tokens = 128000;
    std::string include_pattern;
    std::string exclude_pattern;

    // Options for 'build' and 'test'
    std::vector<std::string> passthrough_args;
    
    // Options for 'model' command
    std::string model_subcommand;  // list, test, info, reload
    std::string model_name;        // For test and info subcommands
};

class CliParser {
public:
    CliParser() = default;

    /**
     * @brief Sets up all CLI commands, options, and flags.
     * @return A shared pointer to the configured CLI::App object.
     */
    std::shared_ptr<CLI::App> setupCli();

    /**
     * @brief Retrieves the parsed command data.
     * @return A const reference to the Commands struct.
     */
    const Commands& getCommands() const;

private:
    void setupInitCommand(CLI::App& app);
    void setupModifyCommand(CLI::App& app);
    void setupAmodifyCommand(CLI::App& app);
    void setupRefactorCommand(CLI::App& app);
    void setupBuildCommand(CLI::App& app);
    void setupTestCommand(CLI::App& app);
    void setupCommitCommand(CLI::App& app);
    void setupPushCommand(CLI::App& app);
    void setupModelCommand(CLI::App& app);

    std::shared_ptr<CLI::App> m_app;
    Commands m_commands;
};

} // namespace Camus

