// =================================================================
// include/Camus/ConfigParser.hpp (New File)
// =================================================================
// Defines a simple parser for the .camus/config.yml file.

#pragma once

#include <string>
#include <map>

namespace Camus {

class ConfigParser {
public:
    /**
     * @brief Constructs the parser and loads the configuration file.
     * @param config_path The path to the config.yml file.
     */
    explicit ConfigParser(const std::string& config_path);

    /**
     * @brief Retrieves a string value for a given key.
     * @param key The configuration key (e.g., "model_path").
     * @return The corresponding value, or an empty string if not found.
     */
    std::string getStringValue(const std::string& key) const;

private:
    std::map<std::string, std::string> m_config_values;
};

} // namespace Camus
