// =================================================================
// src/Camus/CliParser.cpp
// =================================================================
// Implementation for the CLI parser.

#include "Camus/CliParser.hpp"

namespace Camus {

std::shared_ptr<CLI::App> CliParser::setupCli() {
    m_app = std::make_shared<CLI::App>("Camus: A C++-based, local-first AI coding assistant.");
    
    // Set command callback to store which subcommand was used
    m_app->callback([this]() {
        for (auto* subcommand : m_app->get_subcommands()) {
            if (subcommand->parsed()) {
                m_commands.active_command = subcommand->get_name();
                break;
            }
        }
    });

    // Define all commands
    setupInitCommand(*m_app);
    setupModifyCommand(*m_app);
    setupAmodifyCommand(*m_app);
    setupRefactorCommand(*m_app);
    setupBuildCommand(*m_app);
    setupTestCommand(*m_app);
    setupCommitCommand(*m_app);
    setupPushCommand(*m_app);
    setupModelCommand(*m_app);

    return m_app;
}

const Commands& CliParser::getCommands() const {
    return m_commands;
}

void CliParser::setupInitCommand(CLI::App& app) {
    app.add_subcommand("init", "Initializes Camus configuration in the current project.");
}

void CliParser::setupModifyCommand(CLI::App& app) {
    auto* sub = app.add_subcommand("modify", "Modifies a source file based on a natural language prompt.");
    sub->add_option("prompt", m_commands.prompt, "The prompt describing the desired code change.")->required();
    sub->add_option("-f,--file", m_commands.file_path, "The path to the source file to modify.")->required()->check(CLI::ExistingFile);
}

void CliParser::setupAmodifyCommand(CLI::App& app) {
    auto* sub = app.add_subcommand("amodify", "Modifies multiple files across the project based on a high-level request.");
    sub->add_option("prompt", m_commands.prompt, "The high-level request (e.g., 'add user authentication system').")->required();
    sub->add_option("--max-files", m_commands.max_files, "Maximum number of files to include in context (default: 100)");
    sub->add_option("--max-tokens", m_commands.max_tokens, "Maximum tokens for LLM context (default: 128000)");
    sub->add_option("--include", m_commands.include_pattern, "Include only files matching this pattern (e.g., 'src/**/*.cpp')");
    sub->add_option("--exclude", m_commands.exclude_pattern, "Exclude files matching this pattern");
}

void CliParser::setupRefactorCommand(CLI::App& app) {
    auto* sub = app.add_subcommand("refactor", "Applies specific refactoring to a source file.");
    sub->add_option("style", m_commands.prompt, "The refactoring instruction (e.g., 'extract to function').")->required();
    sub->add_option("-f,--file", m_commands.file_path, "The path to the source file to refactor.")->required()->check(CLI::ExistingFile);
}

void CliParser::setupBuildCommand(CLI::App& app) {
    auto* sub = app.add_subcommand("build", "Compiles the project using the configured build command.");
    sub->add_option("args", m_commands.passthrough_args, "Arguments to pass to the build tool.");
}

void CliParser::setupTestCommand(CLI::App& app) {
    auto* sub = app.add_subcommand("test", "Runs the project's tests using the configured test command.");
    sub->add_option("args", m_commands.passthrough_args, "Arguments to pass to the test runner.");
}

void CliParser::setupCommitCommand(CLI::App& app) {
    app.add_subcommand("commit", "Generates a commit message for staged changes and commits them.");
}

void CliParser::setupPushCommand(CLI::App& app) {
    app.add_subcommand("push", "Pushes committed changes to the remote repository.");
}

void CliParser::setupModelCommand(CLI::App& app) {
    auto* model_cmd = app.add_subcommand("model", "Manage and interact with language models");
    
    // List subcommand
    auto* list_cmd = model_cmd->add_subcommand("list", "List all configured models and their status");
    list_cmd->callback([this]() { m_commands.model_subcommand = "list"; });
    
    // Test subcommand
    auto* test_cmd = model_cmd->add_subcommand("test", "Test a specific model or all models");
    test_cmd->add_option("model", m_commands.model_name, "Model name to test (leave empty to test all)");
    test_cmd->callback([this]() { m_commands.model_subcommand = "test"; });
    
    // Info subcommand
    auto* info_cmd = model_cmd->add_subcommand("info", "Show detailed information about a model");
    info_cmd->add_option("model", m_commands.model_name, "Model name to get info for")->required();
    info_cmd->callback([this]() { m_commands.model_subcommand = "info"; });
    
    // Reload subcommand
    auto* reload_cmd = model_cmd->add_subcommand("reload", "Reload model configuration from file");
    reload_cmd->callback([this]() { m_commands.model_subcommand = "reload"; });
    
    // Make model command trigger active_command
    model_cmd->callback([this]() { m_commands.active_command = "model"; });
}

} // namespace Camus


