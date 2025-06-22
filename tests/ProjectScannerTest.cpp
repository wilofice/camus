// =================================================================
// tests/ProjectScannerTest.cpp
// =================================================================
// Unit tests for ProjectScanner component.

#include "Camus/ProjectScanner.hpp"
#include "Camus/IgnorePattern.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

class ProjectScannerTest {
private:
    std::string test_dir;
    
    void setupTestFiles() {
        // Create test directory structure
        fs::create_directories(test_dir + "/src");
        fs::create_directories(test_dir + "/build");
        fs::create_directories(test_dir + "/docs");
        fs::create_directories(test_dir + "/.git");
        
        // Create test files
        std::ofstream(test_dir + "/src/main.cpp") << "int main() { return 0; }";
        std::ofstream(test_dir + "/src/utils.hpp") << "#pragma once";
        std::ofstream(test_dir + "/src/test.py") << "print('hello')";
        std::ofstream(test_dir + "/build/output.o") << "binary data";
        std::ofstream(test_dir + "/docs/README.md") << "# Documentation";
        std::ofstream(test_dir + "/.git/config") << "[core]";
        std::ofstream(test_dir + "/CMakeLists.txt") << "cmake_minimum_required(VERSION 3.10)";
    }
    
    void cleanupTestFiles() {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }
    
public:
    ProjectScannerTest() : test_dir("test_project_scanner") {}
    
    void testBasicScanning() {
        std::cout << "Testing basic file scanning..." << std::endl;
        
        setupTestFiles();
        
        Camus::ProjectScanner scanner(test_dir);
        auto files = scanner.scanFiles();
        
        // Should find all files by default
        assert(!files.empty());
        
        // Check that we found expected files
        bool found_main = false, found_cmake = false;
        for (const auto& file : files) {
            if (file.find("main.cpp") != std::string::npos) found_main = true;
            if (file.find("CMakeLists.txt") != std::string::npos) found_cmake = true;
        }
        
        assert(found_main && "Should find main.cpp");
        assert(found_cmake && "Should find CMakeLists.txt");
        
        cleanupTestFiles();
        std::cout << "✓ Basic scanning test passed" << std::endl;
    }
    
    void testExtensionFiltering() {
        std::cout << "Testing extension filtering..." << std::endl;
        
        setupTestFiles();
        
        Camus::ProjectScanner scanner(test_dir);
        scanner.addIncludeExtension(".cpp");
        scanner.addIncludeExtension(".hpp");
        
        auto files = scanner.scanFiles();
        
        // Should only find C++ files
        for (const auto& file : files) {
            bool is_cpp = (file.size() >= 4 && file.substr(file.size() - 4) == ".cpp") || 
                         (file.size() >= 4 && file.substr(file.size() - 4) == ".hpp");
            assert(is_cpp && "Should only find C++ files");
        }
        
        cleanupTestFiles();
        std::cout << "✓ Extension filtering test passed" << std::endl;
    }
    
    void testIgnorePatterns() {
        std::cout << "Testing ignore patterns..." << std::endl;
        
        setupTestFiles();
        
        Camus::ProjectScanner scanner(test_dir);
        scanner.addIgnorePattern("build/");
        scanner.addIgnorePattern(".git/");
        scanner.addIgnorePattern("*.o");
        
        auto files = scanner.scanFiles();
        
        // Should not find files in ignored directories or with ignored extensions
        for (const auto& file : files) {
            assert(file.find("/build/") == std::string::npos && "Should ignore build directory");
            assert(file.find("/.git/") == std::string::npos && "Should ignore .git directory");
            assert(!(file.size() >= 2 && file.substr(file.size() - 2) == ".o") && "Should ignore .o files");
        }
        
        cleanupTestFiles();
        std::cout << "✓ Ignore patterns test passed" << std::endl;
    }
    
    void testFileSizeLimit() {
        std::cout << "Testing file size limit..." << std::endl;
        
        setupTestFiles();
        
        // Create a large file
        {
            std::ofstream large_file(test_dir + "/large.txt");
            for (int i = 0; i < 200000; i++) {  // Create ~200KB file
                large_file << "This is line " << i << " of the large file.\n";
            }
        }
        
        Camus::ProjectScanner scanner(test_dir);
        scanner.setMaxFileSize(100 * 1024);  // 100KB limit
        
        auto files = scanner.scanFiles();
        
        // Should not include the large file
        bool found_large = false;
        for (const auto& file : files) {
            if (file.find("large.txt") != std::string::npos) {
                found_large = true;
            }
        }
        
        assert(!found_large && "Should exclude files larger than size limit");
        
        cleanupTestFiles();
        std::cout << "✓ File size limit test passed" << std::endl;
    }
    
    void testEmptyDirectory() {
        std::cout << "Testing empty directory..." << std::endl;
        
        fs::create_directory(test_dir);
        
        Camus::ProjectScanner scanner(test_dir);
        auto files = scanner.scanFiles();
        
        assert(files.empty() && "Empty directory should return no files");
        
        cleanupTestFiles();
        std::cout << "✓ Empty directory test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running ProjectScanner unit tests..." << std::endl;
        
        testBasicScanning();
        testExtensionFiltering();
        testIgnorePatterns();
        testFileSizeLimit();
        testEmptyDirectory();
        
        std::cout << "All ProjectScanner tests passed!" << std::endl;
    }
};

// Test IgnorePattern class as well
class IgnorePatternTest {
public:
    void testBasicMatching() {
        std::cout << "Testing IgnorePattern basic matching..." << std::endl;
        
        Camus::IgnorePattern pattern("*.txt");
        
        assert(pattern.matches("file.txt") && "Should match *.txt pattern");
        assert(!pattern.matches("file.cpp") && "Should not match non-txt files");
        assert(pattern.matches("path/to/file.txt") && "Should match in subdirectories");
        
        std::cout << "✓ Basic matching test passed" << std::endl;
    }
    
    void testDirectoryMatching() {
        std::cout << "Testing directory pattern matching..." << std::endl;
        
        Camus::IgnorePattern pattern("build/");
        
        assert(pattern.matches("build/", true) && "Should match directory itself");
        assert(pattern.matches("build/file.o") && "Should match files in directory");
        assert(pattern.matches("project/build/output") && "Should match nested directories");
        assert(!pattern.matches("buildfile.txt") && "Should not match files starting with pattern");
        
        std::cout << "✓ Directory matching test passed" << std::endl;
    }
    
    void testNegationPatterns() {
        std::cout << "Testing negation patterns..." << std::endl;
        
        Camus::IgnorePattern pattern("!important.txt");
        
        assert(!pattern.matches("important.txt") && "Negation should not match negated pattern");
        assert(pattern.matches("other.txt") && "Should match non-negated files");
        
        std::cout << "✓ Negation patterns test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running IgnorePattern unit tests..." << std::endl;
        
        testBasicMatching();
        testDirectoryMatching();
        testNegationPatterns();
        
        std::cout << "All IgnorePattern tests passed!" << std::endl;
    }
};

int main() {
    try {
        ProjectScannerTest scanner_tests;
        scanner_tests.runAllTests();
        
        std::cout << std::endl;
        
        IgnorePatternTest pattern_tests;
        pattern_tests.runAllTests();
        
        std::cout << "\n🎉 All ProjectScanner component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}