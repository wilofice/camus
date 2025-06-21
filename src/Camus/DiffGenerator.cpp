#include "Camus/DiffGenerator.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace Camus {

DiffGenerator::DiffGenerator(const std::string& original, const std::string& modified)
: m_original(original), m_modified(modified) {
}

std::vector<std::string> DiffGenerator::splitIntoLines(const std::string& text) const {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    
    if (!text.empty() && text.back() != '\n' && lines.empty()) {
        lines.push_back(text);
    }
    
    return lines;
}

std::vector<DiffLine> DiffGenerator::computeDiff() const {
    std::vector<DiffLine> diff;
    auto original_lines = splitIntoLines(m_original);
    auto modified_lines = splitIntoLines(m_modified);
    
    size_t i = 0, j = 0;
    
    while (i < original_lines.size() || j < modified_lines.size()) {
        if (i >= original_lines.size()) {
            diff.push_back({modified_lines[j], DiffLineType::ADDED, 0, j + 1});
            j++;
        } else if (j >= modified_lines.size()) {
            diff.push_back({original_lines[i], DiffLineType::REMOVED, i + 1, 0});
            i++;
        } else if (original_lines[i] == modified_lines[j]) {
            diff.push_back({original_lines[i], DiffLineType::UNCHANGED, i + 1, j + 1});
            i++;
            j++;
        } else {
            size_t look_ahead = 3;
            bool found_match = false;
            
            for (size_t k = 1; k <= look_ahead && j + k < modified_lines.size(); k++) {
                if (original_lines[i] == modified_lines[j + k]) {
                    for (size_t l = 0; l < k; l++) {
                        diff.push_back({modified_lines[j + l], DiffLineType::ADDED, 0, j + l + 1});
                    }
                    j += k;
                    found_match = true;
                    break;
                }
            }
            
            if (!found_match) {
                for (size_t k = 1; k <= look_ahead && i + k < original_lines.size(); k++) {
                    if (original_lines[i + k] == modified_lines[j]) {
                        for (size_t l = 0; l < k; l++) {
                            diff.push_back({original_lines[i + l], DiffLineType::REMOVED, i + l + 1, 0});
                        }
                        i += k;
                        found_match = true;
                        break;
                    }
                }
            }
            
            if (!found_match) {
                diff.push_back({original_lines[i], DiffLineType::REMOVED, i + 1, 0});
                diff.push_back({modified_lines[j], DiffLineType::ADDED, 0, j + 1});
                i++;
                j++;
            }
        }
    }
    
    return diff;
}

std::vector<DiffLine> DiffGenerator::getDiff() const {
    return computeDiff();
}

void DiffGenerator::printColoredDiff() const {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string GRAY = "\033[90m";
    
    auto diff_lines = getDiff();
    
    std::cout << "\n" << GRAY << "=== Diff View ===" << RESET << "\n\n";
    
    for (const auto& line : diff_lines) {
        switch (line.type) {
            case DiffLineType::ADDED:
                std::cout << GREEN << "+ " << line.text << RESET << "\n";
                break;
            case DiffLineType::REMOVED:
                std::cout << RED << "- " << line.text << RESET << "\n";
                break;
            case DiffLineType::UNCHANGED:
                std::cout << "  " << line.text << "\n";
                break;
        }
    }
    
    std::cout << "\n" << GRAY << "================" << RESET << "\n";
}
}
