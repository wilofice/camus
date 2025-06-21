#ifndef CAMUS_DIFFGENERATOR_HPP
#define CAMUS_DIFFGENERATOR_HPP

#include <string>
#include <vector>

namespace Camus {

enum class DiffLineType {
    UNCHANGED,
    ADDED,
    REMOVED
};

struct DiffLine {
    std::string text;
    DiffLineType type;
    size_t original_line_num;
    size_t new_line_num;
};

class DiffGenerator {
public:
    DiffGenerator(const std::string& original, const std::string& modified);
    
    std::vector<DiffLine> getDiff() const;
    
    void printColoredDiff() const;
    
private:
    std::vector<std::string> splitIntoLines(const std::string& text) const;
    std::vector<DiffLine> computeDiff() const;
    
    std::string m_original;
    std::string m_modified;
};

} // namespace Camus

#endif // CAMUS_DIFFGENERATOR_HPP