// =================================================================
// tests/ContextBuilderTest.cpp
// =================================================================
// Unit tests for ContextBuilder component.

#include "Camus/ContextBuilder.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

class ContextBuilderTest {
private:
    std::string test_dir;
    
    void setupTestFiles() {
        fs::create_directories(test_dir);
        
        // Create test files with different sizes
        std::ofstream(test_dir + "/small.cpp") << "int main() { return 0; }";
        
        std::ofstream(test_dir + "/medium.cpp") << R"(
#include <iostream>
#include <vector>

class TestClass {
public:
    void doSomething() {
        std::cout << "Hello, World!" << std::endl;
    }
    
    int calculate(int a, int b) {
        return a + b;
    }
};

int main() {
    TestClass test;
    test.doSomething();
    return test.calculate(5, 10);
}
)";
        
        // Create a larger file
        std::ofstream large_file(test_dir + "/large.cpp");
        large_file << "#include <iostream>\n\n";
        for (int i = 0; i < 100; i++) {
            large_file << "void function" << i << "() {\n";
            large_file << "    // This is function " << i << "\n";
            large_file << "    std::cout << \"Function " << i << "\" << std::endl;\n";
            large_file << "}\n\n";
        }
        large_file.close();
    }
    
    void cleanupTestFiles() {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }
    
public:
    ContextBuilderTest() : test_dir("test_context_builder") {}
    
    void testBasicContextBuilding() {
        std::cout << "Testing basic context building..." << std::endl;
        
        setupTestFiles();
        
        std::vector<std::string> files = {
            test_dir + "/small.cpp",
            test_dir + "/medium.cpp"
        };
        
        Camus::ContextBuilder builder(10000);  // 10k token limit
        std::string context = builder.buildContext(files, "Fix compilation errors", test_dir);
        
        assert(!context.empty() && "Context should not be empty");
        assert(context.find("small.cpp") != std::string::npos && "Should include small.cpp");
        assert(context.find("medium.cpp") != std::string::npos && "Should include medium.cpp");
        assert(context.find("Fix compilation errors") != std::string::npos && "Should include user request");
        
        auto stats = builder.getLastBuildStats();
        assert(stats["files_included"] == 2 && "Should include 2 files");
        assert(stats["tokens_used"] > 0 && "Should report token usage");
        
        cleanupTestFiles();
        std::cout << "✓ Basic context building test passed" << std::endl;
    }
    
    void testTokenLimitRespected() {
        std::cout << "Testing token limit enforcement..." << std::endl;
        
        setupTestFiles();
        
        std::vector<std::string> files = {
            test_dir + "/small.cpp",
            test_dir + "/medium.cpp",
            test_dir + "/large.cpp"
        };
        
        Camus::ContextBuilder builder(1000);  // Very small token limit
        std::string context = builder.buildContext(files, "Test request", test_dir);
        
        auto stats = builder.getLastBuildStats();
        assert(stats["tokens_used"] <= 1000 && "Should respect token limit");
        
        // With such a small limit, large.cpp should be truncated or excluded
        assert(stats["files_truncated"] > 0 || stats["files_included"] < 3 && 
               "Should truncate or exclude files to fit limit");
        
        cleanupTestFiles();
        std::cout << "✓ Token limit test passed" << std::endl;
    }
    
    void testFilePrioritization() {
        std::cout << "Testing file prioritization..." << std::endl;
        
        setupTestFiles();
        
        // Add a recently modified file
        std::ofstream(test_dir + "/recent.cpp") << "// Recently modified file\nint main() {}";
        
        std::vector<std::string> files = {
            test_dir + "/small.cpp",
            test_dir + "/medium.cpp",
            test_dir + "/large.cpp",
            test_dir + "/recent.cpp"
        };
        
        Camus::ContextBuilder builder(2000);  // Limited tokens to force prioritization
        builder.setGitPrioritization(true);
        
        std::string context = builder.buildContext(files, "Test request", test_dir);
        
        auto stats = builder.getLastBuildStats();
        
        // Should prioritize smaller, more important files
        assert(context.find("small.cpp") != std::string::npos && "Should prioritize small files");
        assert(stats["files_included"] > 0 && "Should include some files");
        
        cleanupTestFiles();
        std::cout << "✓ File prioritization test passed" << std::endl;
    }
    
    void testRelevanceKeywords() {
        std::cout << "Testing relevance keyword scoring..." << std::endl;
        
        setupTestFiles();
        
        // Create a file with specific keywords
        std::ofstream(test_dir + "/relevant.cpp") << R"(
#include <iostream>
#include <vector>

// This file contains error handling and debugging functionality
void handleError(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
}

void debugPrint(const std::string& info) {
    std::cout << "Debug: " << info << std::endl;
}
)";
        
        std::vector<std::string> files = {
            test_dir + "/small.cpp",
            test_dir + "/medium.cpp",
            test_dir + "/relevant.cpp"
        };
        
        Camus::ContextBuilder builder(5000);
        builder.setRelevanceKeywords({"error", "debug", "handling"});
        
        std::string context = builder.buildContext(files, "Fix error handling", test_dir);
        
        // The relevant.cpp file should be prioritized due to keyword matches
        assert(context.find("relevant.cpp") != std::string::npos && 
               "Should prioritize files with relevant keywords");
        
        cleanupTestFiles();
        std::cout << "✓ Relevance keywords test passed" << std::endl;
    }
    
    void testEmptyFileList() {
        std::cout << "Testing empty file list..." << std::endl;
        
        std::vector<std::string> files;
        
        Camus::ContextBuilder builder(1000);
        std::string context = builder.buildContext(files, "Test request", ".");
        
        assert(!context.empty() && "Should still include user request even with no files");
        assert(context.find("Test request") != std::string::npos && "Should include user request");
        
        auto stats = builder.getLastBuildStats();
        assert(stats["files_included"] == 0 && "Should report 0 files included");
        
        std::cout << "✓ Empty file list test passed" << std::endl;
    }
    
    void testVeryLowTokenLimit() {
        std::cout << "Testing very low token limit..." << std::endl;
        
        setupTestFiles();
        
        std::vector<std::string> files = {test_dir + "/small.cpp"};
        
        Camus::ContextBuilder builder(50);  // Extremely low limit
        std::string context = builder.buildContext(files, "Test", test_dir);
        
        auto stats = builder.getLastBuildStats();
        assert(stats["tokens_used"] <= 50 && "Should respect even very low limits");
        
        cleanupTestFiles();
        std::cout << "✓ Very low token limit test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running ContextBuilder unit tests..." << std::endl;
        
        testBasicContextBuilding();
        testTokenLimitRespected();
        testFilePrioritization();
        testRelevanceKeywords();
        testEmptyFileList();
        testVeryLowTokenLimit();
        
        std::cout << "All ContextBuilder tests passed!" << std::endl;
    }
};

int main() {
    try {
        ContextBuilderTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All ContextBuilder component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}