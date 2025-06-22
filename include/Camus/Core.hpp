// =================================================================
// include/Camus/Core.hpp
// =================================================================
// Defines the core application orchestrator.

#pragma once

#include "Camus/CliParser.hpp"
#include <memory>
#include <string>

// Forward declarations to reduce header dependencies
namespace Camus {
    class ConfigParser;
    class LlmInteraction;
    class SysInteraction;
}

namespace Camus {

class Core {
public:
    /**
     * @brief Constructs the Core application object.
     * @param commands The parsed command-line arguments.
     */
    explicit Core(const Commands& commands);

    /**
     * @brief Destructor must be declared here and defined in the .cpp file.
     * This is required because we are using unique_ptr with forward-declared types.
     */
    ~Core();

    /**
     * @brief Runs the main application logic based on parsed commands.
     * @return An integer exit code (0 for success).
     */
    int run();

private:
    // Command Handlers
    int handleInit();
    int handleModify();
    int handleAmodify();
    int handleRefactor();
    int handleBuild();
    int handleTest();
    int handleCommit();
    int handlePush();

    const Commands& m_commands;
    std::unique_ptr<ConfigParser> m_config;
    std::unique_ptr<LlmInteraction> m_llm;
    std::unique_ptr<SysInteraction> m_sys;
};

} // namespace Camus
