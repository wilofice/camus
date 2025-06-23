// =================================================================
// src/Camus/ModelCapabilities.cpp
// =================================================================
// Implementation of model capabilities and metadata utilities.

#include "Camus/ModelCapabilities.hpp"
#include <stdexcept>
#include <algorithm>
#include <unordered_map>

namespace Camus {

std::string ModelCapabilityUtils::capabilityToString(ModelCapability capability) {
    switch (capability) {
        case ModelCapability::FAST_INFERENCE:
            return "FAST_INFERENCE";
        case ModelCapability::HIGH_QUALITY:
            return "HIGH_QUALITY";
        case ModelCapability::CODE_SPECIALIZED:
            return "CODE_SPECIALIZED";
        case ModelCapability::LARGE_CONTEXT:
            return "LARGE_CONTEXT";
        case ModelCapability::SECURITY_FOCUSED:
            return "SECURITY_FOCUSED";
        case ModelCapability::MULTILINGUAL:
            return "MULTILINGUAL";
        case ModelCapability::REASONING:
            return "REASONING";
        case ModelCapability::CREATIVE_WRITING:
            return "CREATIVE_WRITING";
        case ModelCapability::INSTRUCTION_TUNED:
            return "INSTRUCTION_TUNED";
        case ModelCapability::CHAT_OPTIMIZED:
            return "CHAT_OPTIMIZED";
        default:
            throw std::invalid_argument("Unknown ModelCapability value");
    }
}

ModelCapability ModelCapabilityUtils::stringToCapability(const std::string& str) {
    static const std::unordered_map<std::string, ModelCapability> capability_map = {
        {"FAST_INFERENCE", ModelCapability::FAST_INFERENCE},
        {"HIGH_QUALITY", ModelCapability::HIGH_QUALITY},
        {"CODE_SPECIALIZED", ModelCapability::CODE_SPECIALIZED},
        {"LARGE_CONTEXT", ModelCapability::LARGE_CONTEXT},
        {"SECURITY_FOCUSED", ModelCapability::SECURITY_FOCUSED},
        {"MULTILINGUAL", ModelCapability::MULTILINGUAL},
        {"REASONING", ModelCapability::REASONING},
        {"CREATIVE_WRITING", ModelCapability::CREATIVE_WRITING},
        {"INSTRUCTION_TUNED", ModelCapability::INSTRUCTION_TUNED},
        {"CHAT_OPTIMIZED", ModelCapability::CHAT_OPTIMIZED}
    };
    
    auto it = capability_map.find(str);
    if (it != capability_map.end()) {
        return it->second;
    }
    
    throw std::invalid_argument("Unknown capability string: " + str);
}

std::vector<ModelCapability> ModelCapabilityUtils::getAllCapabilities() {
    return {
        ModelCapability::FAST_INFERENCE,
        ModelCapability::HIGH_QUALITY,
        ModelCapability::CODE_SPECIALIZED,
        ModelCapability::LARGE_CONTEXT,
        ModelCapability::SECURITY_FOCUSED,
        ModelCapability::MULTILINGUAL,
        ModelCapability::REASONING,
        ModelCapability::CREATIVE_WRITING,
        ModelCapability::INSTRUCTION_TUNED,
        ModelCapability::CHAT_OPTIMIZED
    };
}

bool ModelCapabilityUtils::hasCapability(const ModelMetadata& metadata, ModelCapability capability) {
    return std::find(metadata.capabilities.begin(), metadata.capabilities.end(), capability) 
           != metadata.capabilities.end();
}

std::vector<std::string> ModelCapabilityUtils::capabilitiesToStrings(const std::vector<ModelCapability>& capabilities) {
    std::vector<std::string> result;
    result.reserve(capabilities.size());
    
    for (const auto& capability : capabilities) {
        result.push_back(capabilityToString(capability));
    }
    
    return result;
}

std::vector<ModelCapability> ModelCapabilityUtils::parseCapabilities(const std::vector<std::string>& capability_strings) {
    std::vector<ModelCapability> result;
    result.reserve(capability_strings.size());
    
    for (const auto& str : capability_strings) {
        result.push_back(stringToCapability(str));
    }
    
    return result;
}

} // namespace Camus