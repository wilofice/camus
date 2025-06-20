// =================================================================
// include/Camus/SysInteraction.hpp
// =================================================================
// Defines the interface for system-level operations like file I/O
// and running external processes.

#pragma once

#include <string>
#include <vector>
#include <utility> // For std::pair

namespace Camus {

class SysInteraction {
public:
    /**
     * @brief Reads the entire content of a file into a string.
     * @param file_path The path to the file.
     * @return The content of the file. Throws std::runtime_error on failure.
     */
    std::string readFile(const std::string& file_path);

    /**
     * @brief Writes content to a file, overwriting it.
     * @param file_path The path to the file.
     * @param content The content to write.
     * @return True on success, false on failure.
     */
    bool writeFile(const std::string& file_path, const std::string& content);

    /**
     * @brief Checks if a file exists.
     */
    bool fileExists(const std::string& file_path);
    
    /**
     * @brief Checks if a directory exists.
     */
    bool directoryExists(const std::string& dir_path);

    /**
     * @brief Creates a directory.
     */
    bool createDirectory(const std::string& dir_path);


    /**
     * @brief Executes an external command and captures its output.
     * @param command The command to execute.
     * @param args A vector of arguments for the command.
     * @return A pair containing the stdout and the exit code.
     */
    std::pair<std::string, int> executeCommand(const std::string& command, const std::vector<std::string>& args);
};

} // namespace Camus

