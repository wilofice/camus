// =================================================================
// src/Camus/SysInteraction.cpp
// =================================================================
// Implementation for system-level operations.

#include "Camus/SysInteraction.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif


namespace Camus {

std::string SysInteraction::readFile(const std::string& file_path) {
    std::ifstream file_stream(file_path);
    if (!file_stream) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
    std::stringstream buffer;
    buffer << file_stream.rdbuf();
    return buffer.str();
}

bool SysInteraction::writeFile(const std::string& file_path, const std::string& content) {
    std::ofstream file_stream(file_path);
    if (!file_stream) {
        return false;
    }
    file_stream << content;
    return file_stream.good();
}

bool SysInteraction::fileExists(const std::string& file_path) {
    struct stat buffer;
    return (stat(file_path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
}

bool SysInteraction::directoryExists(const std::string& dir_path) {
    struct stat buffer;
    return (stat(dir_path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

bool SysInteraction::createDirectory(const std::string& dir_path) {
    int status = 0;
#if defined(_WIN32)
    status = _mkdir(dir_path.c_str());
#else
    status = mkdir(dir_path.c_str(), 0755); // Read, write, execute for owner, read/execute for others
#endif
    return status == 0;
}

std::pair<std::string, int> SysInteraction::executeCommand(const std::string& command, const std::vector<std::string>& args) {
    // This is a stub and does not actually execute a command.
    // A real implementation would use platform-specific APIs like popen or CreateProcess.
    return {"Command executed successfully (stub).", 0};
}

} // namespace Camus

