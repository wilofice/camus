// =================================================================
// tests/IntegrationTest.cpp
// =================================================================
// Integration tests for Camus amodify workflow.

#include "Camus/ProjectScanner.hpp"
#include "Camus/ContextBuilder.hpp"
#include "Camus/ResponseParser.hpp"
#include "Camus/SafetyChecker.hpp"
#include "Camus/BackupManager.hpp"
#include "Camus/AmodifyConfig.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <vector>

class IntegrationTest {
private:
    std::string test_project_dir;
    
    void setupTestProject() {
        // Create a realistic test project structure
        std::filesystem::create_directories(test_project_dir + "/src");
        std::filesystem::create_directories(test_project_dir + "/include");
        std::filesystem::create_directories(test_project_dir + "/tests");
        std::filesystem::create_directories(test_project_dir + "/build");
        
        // Create realistic source files
        std::ofstream(test_project_dir + "/src/main.cpp") << R"(
#include <iostream>
#include "utils.hpp"

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
)";
        
        std::ofstream(test_project_dir + "/include/utils.hpp") << R"(
#pragma once
#include <string>

std::string getMessage();
void processData(int value);
)";
        
        std::ofstream(test_project_dir + "/src/utils.cpp") << R"(
#include "utils.hpp"
#include <iostream>

std::string getMessage() {
    return "Hello from utils!";
}

void processData(int value) {
    std::cout << "Processing: " << value << std::endl;
}
)";
        
        std::ofstream(test_project_dir + "/CMakeLists.txt") << R"(
cmake_minimum_required(VERSION 3.10)
project(TestProject)

add_executable(test_app src/main.cpp src/utils.cpp)
target_include_directories(test_app PRIVATE include)
)";
        
        // Create a gitignore file
        std::ofstream(test_project_dir + "/.gitignore") << R"(
build/
*.o
*.exe
)";
    }
    
    void cleanupTestProject() {
        if (std::filesystem::exists(test_project_dir)) {
            std::filesystem::remove_all(test_project_dir);
        }
    }
    
    Camus::AmodifyConfig createTestConfig() {
        Camus::AmodifyConfig config;
        config.max_files = 50;
        config.max_tokens = 10000;
        config.max_modification_size = 1024 * 1024; // 1MB
        config.include_extensions = {".cpp", ".hpp", ".txt"};
        config.default_ignore_patterns = {"build/", "*.o"};
        config.create_backups = true;
        config.backup_dir = test_project_dir + "/.backups";
        config.git_check = false; // Disable for tests
        return config;
    }
    
public:
    IntegrationTest() : test_project_dir("integration_test_project") {}
    
    void testCompleteWorkflow() {
        std::cout << "Testing complete amodify workflow integration..." << std::endl;
        
        setupTestProject();
        
        try {
            // Step 1: Scan project files
            std::cout << "  Step 1: Scanning project files..." << std::endl;
            Camus::ProjectScanner scanner(test_project_dir);
            scanner.addIncludeExtension(".cpp");
            scanner.addIncludeExtension(".hpp");
            scanner.addIgnorePattern("build/");
            
            auto files = scanner.scanFiles();
            assert(!files.empty() && "Should find source files");
            std::cout << "    Found " << files.size() << " files" << std::endl;
            
            // Step 2: Build context
            std::cout << "  Step 2: Building context..." << std::endl;
            Camus::ContextBuilder builder(5000);
            std::string context = builder.buildContext(files, "Add error handling to all functions", test_project_dir);
            assert(!context.empty() && "Should build context");
            
            auto stats = builder.getLastBuildStats();
            std::cout << "    Context: " << stats["files_included"] << " files, " 
                      << stats["tokens_used"] << " tokens" << std::endl;
            
            // Step 3: Simulate LLM response parsing
            std::cout << "  Step 3: Parsing simulated LLM response..." << std::endl;
            std::string simulated_response = R"(
I'll add error handling to your functions:

==== FILE: src/utils.cpp ====
#include "utils.hpp"
#include <iostream>
#include <stdexcept>

std::string getMessage() {
    try {
        return "Hello from utils!";
    } catch (const std::exception& e) {
        std::cerr << "Error in getMessage: " << e.what() << std::endl;
        return "";
    }
}

void processData(int value) {
    try {
        if (value < 0) {
            throw std::invalid_argument("Value must be non-negative");
        }
        std::cout << "Processing: " << value << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error processing data: " << e.what() << std::endl;
    }
}

==== FILE: include/utils.hpp ====
#pragma once
#include <string>
#include <stdexcept>

std::string getMessage();
void processData(int value);

==== END ====
)";
            
            Camus::ResponseParser parser(test_project_dir);
            auto modifications = parser.parseResponse(simulated_response);
            assert(modifications.size() == 2 && "Should parse 2 file modifications");
            std::cout << "    Parsed " << modifications.size() << " modifications" << std::endl;
            
            // Step 4: Safety checks
            std::cout << "  Step 4: Performing safety checks..." << std::endl;
            Camus::SafetyChecker checker(test_project_dir);
            auto config = createTestConfig();
            auto safety_report = checker.performSafetyChecks(modifications, config);
            assert(safety_report.can_proceed && "Safety checks should pass for normal modifications");
            std::cout << "    Safety level: " << Camus::SafetyChecker::getSafetyLevelDescription(safety_report.overall_level) << std::endl;
            
            // Step 5: Create backups
            std::cout << "  Step 5: Creating backups..." << std::endl;
            Camus::BackupManager backup_manager(config.backup_dir);
            
            std::vector<std::string> files_to_backup;
            for (const auto& mod : modifications) {
                if (!mod.is_new_file) {
                    files_to_backup.push_back(mod.file_path);
                }
            }
            
            size_t backups_created = backup_manager.createBackups(files_to_backup);
            assert(backups_created == files_to_backup.size() && "Should create all backups");
            std::cout << "    Created " << backups_created << " backups" << std::endl;
            
            std::cout << "✓ Complete workflow integration test passed" << std::endl;
            
        } catch (const std::exception& e) {
            cleanupTestProject();
            throw;
        }
        
        cleanupTestProject();
    }
    
    void testErrorHandling() {
        std::cout << "Testing error handling in integrated workflow..." << std::endl;
        
        setupTestProject();
        
        try {
            // Test with malicious LLM response
            std::string malicious_response = R"(
==== FILE: src/malicious.cpp ====
#include <cstdlib>
int main() {
    system("rm -rf /");  // Dangerous!
    return 0;
}
==== END ====
)";
            
            Camus::ResponseParser parser(test_project_dir);
            parser.setStrictValidation(true);
            auto modifications = parser.parseResponse(malicious_response);
            
            if (!modifications.empty()) {
                Camus::SafetyChecker checker(test_project_dir);
                checker.setStrictMode(true);
                auto config = createTestConfig();
                
                auto safety_report = checker.performSafetyChecks(modifications, config);
                
                // Should detect security issues
                assert(safety_report.overall_level >= Camus::SafetyLevel::DANGER && 
                       "Should detect dangerous content");
                
                std::cout << "    ✓ Security issues detected correctly" << std::endl;
            }
            
            std::cout << "✓ Error handling integration test passed" << std::endl;
            
        } catch (const std::exception& e) {
            cleanupTestProject();
            throw;
        }
        
        cleanupTestProject();
    }
    
    void testConfigurationIntegration() {
        std::cout << "Testing configuration integration..." << std::endl;
        
        setupTestProject();
        
        try {
            // Test with different configuration settings
            auto config = createTestConfig();
            config.max_files = 2; // Very restrictive
            config.max_tokens = 1000; // Small token limit
            
            Camus::ProjectScanner scanner(test_project_dir);
            for (const auto& ext : config.include_extensions) {
                scanner.addIncludeExtension(ext);
            }
            for (const auto& pattern : config.default_ignore_patterns) {
                scanner.addIgnorePattern(pattern);
            }
            
            auto files = scanner.scanFiles();
            
            // Apply file limit
            if (files.size() > config.max_files) {
                files.resize(config.max_files);
            }
            
            Camus::ContextBuilder builder(config.max_tokens);
            std::string context = builder.buildContext(files, "Test request", test_project_dir);
            
            auto stats = builder.getLastBuildStats();
            assert(stats["tokens_used"] <= config.max_tokens && "Should respect token limit");
            assert(stats["files_included"] <= config.max_files && "Should respect file limit");
            
            std::cout << "    ✓ Configuration limits respected" << std::endl;
            std::cout << "✓ Configuration integration test passed" << std::endl;
            
        } catch (const std::exception& e) {
            cleanupTestProject();
            throw;
        }
        
        cleanupTestProject();
    }
    
    void testBackupAndRestore() {
        std::cout << "Testing backup and restore integration..." << std::endl;
        
        setupTestProject();
        
        try {
            // Read original content
            std::string original_content;
            {
                std::ifstream file(test_project_dir + "/src/main.cpp");
                std::string line;
                while (std::getline(file, line)) {
                    original_content += line + "\n";
                }
            }
            
            Camus::BackupManager backup_manager(test_project_dir + "/.backups");
            
            // Create backup
            std::string backup_path = backup_manager.createBackup(test_project_dir + "/src/main.cpp");
            assert(!backup_path.empty() && "Should create backup");
            
            // Modify file
            {
                std::ofstream file(test_project_dir + "/src/main.cpp");
                file << "// Modified content\nint main() { return 1; }";
            }
            
            // Restore from backup
            bool restored = backup_manager.restoreFile(test_project_dir + "/src/main.cpp");
            assert(restored && "Should restore file");
            
            // Verify restoration
            std::string restored_content;
            {
                std::ifstream file(test_project_dir + "/src/main.cpp");
                std::string line;
                while (std::getline(file, line)) {
                    restored_content += line + "\n";
                }
            }
            
            assert(restored_content == original_content && "Should restore original content");
            
            std::cout << "    ✓ Backup and restore working correctly" << std::endl;
            std::cout << "✓ Backup and restore integration test passed" << std::endl;
            
        } catch (const std::exception& e) {
            cleanupTestProject();
            throw;
        }
        
        cleanupTestProject();
    }
    
    void runAllTests() {
        std::cout << "Running Integration Tests..." << std::endl;
        std::cout << "=============================" << std::endl << std::endl;
        
        testCompleteWorkflow();
        std::cout << std::endl;
        
        testErrorHandling();
        std::cout << std::endl;
        
        testConfigurationIntegration();
        std::cout << std::endl;
        
        testBackupAndRestore();
        std::cout << std::endl;
        
        std::cout << "All Integration Tests passed!" << std::endl;
    }
};

int main() {
    try {
        IntegrationTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All Integration Tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Integration test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}