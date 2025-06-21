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
#include <cstdio>
#include <memory>
#include <array>

#if defined(_WIN32)
#include <direct.h>
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
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
    // Build the full command string
    std::string full_command = command;
    for (const auto& arg : args) {
        // Basic shell escaping - wrap arguments containing spaces in quotes
        if (arg.find(' ') != std::string::npos) {
            full_command += " \"" + arg + "\"";
        } else {
            full_command += " " + arg;
        }
    }
    
    // Redirect stderr to stdout to capture all output
    full_command += " 2>&1";
    
    // Open pipe to execute command
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to execute command: " + full_command);
    }
    
    // Read output from the command
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    // Get the exit status
    int exit_status = pclose(pipe.release());
    
    // On Unix-like systems, pclose returns the exit status in a special format
    // We need to extract the actual exit code
#if !defined(_WIN32)
    if (WIFEXITED(exit_status)) {
        exit_status = WEXITSTATUS(exit_status);
    } else {
        // Process terminated abnormally
        exit_status = -1;
    }
#endif
    
    return {result, exit_status};
}

} // namespace Camus

