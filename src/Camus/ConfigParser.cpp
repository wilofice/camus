// =================================================================
// src/Camus/ConfigParser.cpp (New File)
// =================================================================
// Implementation for the simple YAML configuration parser.

#include "Camus/ConfigParser.hpp"
#include <fstream>
#include <iostream>

namespace Camus {

// Helper function to trim whitespace from both ends of a string.
static std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) {
        return s;
    }
    size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, (last - first + 1));
}

ConfigParser::ConfigParser(const std::string& config_path) {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        // It's okay if the file doesn't exist, e.g., before `init` is run.
        return;
    }

    std::string line;
    while (std::getline(config_file, line)) {
        // Ignore comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t delimiter_pos = line.find(':');
        if (delimiter_pos != std::string::npos) {
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);
            m_config_values[trim(key)] = trim(value);
        }
    }
}

std::string ConfigParser::getStringValue(const std::string& key) const {
    auto it = m_config_values.find(key);
    if (it != m_config_values.end()) {
        return it->second;
    }
    return ""; // Return empty string if key not found
}

} // namespace Camus
