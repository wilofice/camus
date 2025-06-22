// =================================================================
// tests/SafetyCheckerTest.cpp
// =================================================================
// Unit tests for SafetyChecker component.

#include "Camus/SafetyChecker.hpp"
#include "Camus/ResponseParser.hpp"
#include "Camus/AmodifyConfig.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <filesystem>

class SafetyCheckerTest {
private:
    std::string test_dir;
    
    Camus::FileModification createModification(const std::string& path, 
                                              const std::string& content, 
                                              bool is_new = false) {
        Camus::FileModification mod;
        mod.file_path = path;
        mod.new_content = content;
        mod.is_new_file = is_new;
        return mod;
    }
    
    Camus::AmodifyConfig createTestConfig() {
        Camus::AmodifyConfig config;
        config.max_modification_size = 10 * 1024 * 1024;  // 10MB
        config.git_check = true;
        config.create_backups = true;
        config.backup_dir = ".camus/backups";
        return config;
    }
    
public:
    SafetyCheckerTest() : test_dir(".") {}
    
    void testBasicSafetyChecks() {
        std::cout << "Testing basic safety checks..." << std::endl;
        
        std::vector<Camus::FileModification> modifications = {
            createModification("src/main.cpp", "#include <iostream>\nint main() { return 0; }")
        };
        
        Camus::SafetyChecker checker(test_dir);
        auto config = createTestConfig();
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        assert(!report.checks.empty() && "Should perform multiple safety checks");
        assert(report.overall_level != Camus::SafetyLevel::CRITICAL && 
               "Basic modifications should not be critical");
        
        std::cout << "✓ Basic safety checks test passed" << std::endl;
    }
    
    void testSuspiciousPatternDetection() {
        std::cout << "Testing suspicious pattern detection..." << std::endl;
        
        std::vector<Camus::FileModification> modifications = {
            createModification("malicious.cpp", R"(
#include <cstdlib>
int main() {
    system("rm -rf /");  // Dangerous command
    return 0;
}
)")
        };
        
        Camus::SafetyChecker checker(test_dir);
        auto config = createTestConfig();
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        // Should detect suspicious patterns
        bool found_suspicious_check = false;
        for (const auto& check : report.checks) {
            if (check.check_name == "Suspicious Patterns" && 
                check.level >= Camus::SafetyLevel::DANGER) {
                found_suspicious_check = true;
                break;
            }
        }
        
        assert(found_suspicious_check && "Should detect suspicious patterns");
        assert(report.overall_level >= Camus::SafetyLevel::DANGER && 
               "Suspicious patterns should raise safety level");
        
        std::cout << "✓ Suspicious pattern detection test passed" << std::endl;
    }
    
    void testModificationSizeChecks() {
        std::cout << "Testing modification size checks..." << std::endl;
        
        // Create a very large modification
        std::string large_content;
        for (int i = 0; i < 100000; i++) {
            large_content += "This is line " + std::to_string(i) + " of a very large file.\n";
        }
        
        std::vector<Camus::FileModification> modifications = {
            createModification("large_file.txt", large_content)
        };
        
        Camus::SafetyChecker checker(test_dir);
        auto config = createTestConfig();
        config.max_modification_size = 1024;  // Very small limit: 1KB
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        // Should detect size violation
        bool found_size_check = false;
        for (const auto& check : report.checks) {
            if (check.check_name == "Modification Size" && 
                check.level >= Camus::SafetyLevel::DANGER) {
                found_size_check = true;
                break;
            }
        }
        
        assert(found_size_check && "Should detect size limit violation");
        
        std::cout << "✓ Modification size checks test passed" << std::endl;
    }
    
    void testCredentialDetection() {
        std::cout << "Testing credential detection..." << std::endl;
        
        std::vector<Camus::FileModification> modifications = {
            createModification("config.cpp", R"(
#include <string>

const std::string API_KEY = "sk-1234567890abcdef";  // Hardcoded API key
const std::string PASSWORD = "super_secret_password";
)")
        };
        
        Camus::SafetyChecker checker(test_dir);
        auto config = createTestConfig();
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        // Should detect potential credentials
        bool found_credential_issue = false;
        for (const auto& check : report.checks) {
            if (check.check_name == "Suspicious Patterns" && 
                check.message.find("credentials") != std::string::npos) {
                found_credential_issue = true;
                break;
            }
        }
        
        assert(found_credential_issue && "Should detect potential credentials");
        
        std::cout << "✓ Credential detection test passed" << std::endl;
    }
    
    void testDependencyConflictDetection() {
        std::cout << "Testing dependency conflict detection..." << std::endl;
        
        std::vector<Camus::FileModification> modifications = {
            createModification("CMakeLists.txt", R"(
cmake_minimum_required(VERSION 3.10)
project(TestProject)
# Modified CMake configuration
)"),
            createModification("package.json", R"(
{
  "name": "test-project",
  "dependencies": {
    "new-dependency": "^1.0.0"
  }
}
)")
        };
        
        Camus::SafetyChecker checker(test_dir);
        auto config = createTestConfig();
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        // Should detect dependency file modifications
        bool found_dependency_check = false;
        for (const auto& check : report.checks) {
            if (check.check_name == "Dependency Conflicts") {
                found_dependency_check = true;
                break;
            }
        }
        
        assert(found_dependency_check && "Should check dependency conflicts");
        
        std::cout << "✓ Dependency conflict detection test passed" << std::endl;
    }
    
    void testStrictMode() {
        std::cout << "Testing strict mode..." << std::endl;
        
        std::vector<Camus::FileModification> modifications = {
            createModification("test.cpp", "int main() { return 0; }")
        };
        
        Camus::SafetyChecker checker(test_dir);
        checker.setStrictMode(true);
        auto config = createTestConfig();
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        // In strict mode, even minor issues should be elevated
        assert(report.overall_level <= Camus::SafetyLevel::DANGER && 
               "Strict mode should be more restrictive");
        
        std::cout << "✓ Strict mode test passed" << std::endl;
    }
    
    void testBackupReadinessCheck() {
        std::cout << "Testing backup readiness check..." << std::endl;
        
        std::vector<Camus::FileModification> modifications = {
            createModification("src/main.cpp", "int main() { return 0; }")
        };
        
        Camus::SafetyChecker checker(test_dir);
        auto config = createTestConfig();
        config.create_backups = true;
        config.backup_dir = "/invalid/path/that/does/not/exist";
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        // Should detect backup issues
        bool found_backup_check = false;
        for (const auto& check : report.checks) {
            if (check.check_name == "Backup Readiness") {
                found_backup_check = true;
                break;
            }
        }
        
        assert(found_backup_check && "Should check backup readiness");
        
        std::cout << "✓ Backup readiness check test passed" << std::endl;
    }
    
    void testSafetyLevelUtilities() {
        std::cout << "Testing safety level utilities..." << std::endl;
        
        // Test safety level descriptions
        assert(Camus::SafetyChecker::getSafetyLevelDescription(Camus::SafetyLevel::SAFE) == "Safe");
        assert(Camus::SafetyChecker::getSafetyLevelDescription(Camus::SafetyLevel::WARNING) == "Warning");
        assert(Camus::SafetyChecker::getSafetyLevelDescription(Camus::SafetyLevel::DANGER) == "Danger");
        assert(Camus::SafetyChecker::getSafetyLevelDescription(Camus::SafetyLevel::CRITICAL) == "Critical");
        
        // Test safety level colors (should return ANSI color codes)
        auto safe_color = Camus::SafetyChecker::getSafetyLevelColor(Camus::SafetyLevel::SAFE);
        auto critical_color = Camus::SafetyChecker::getSafetyLevelColor(Camus::SafetyLevel::CRITICAL);
        
        assert(!safe_color.empty() && "Should return color codes");
        assert(!critical_color.empty() && "Should return color codes");
        assert(safe_color != critical_color && "Different levels should have different colors");
        
        std::cout << "✓ Safety level utilities test passed" << std::endl;
    }
    
    void testEmptyModificationList() {
        std::cout << "Testing empty modification list..." << std::endl;
        
        std::vector<Camus::FileModification> modifications;  // Empty list
        
        Camus::SafetyChecker checker(test_dir);
        auto config = createTestConfig();
        
        auto report = checker.performSafetyChecks(modifications, config);
        
        // Should still perform checks and return safe status
        assert(!report.checks.empty() && "Should still perform some checks");
        assert(report.overall_level == Camus::SafetyLevel::SAFE && 
               "Empty modifications should be safe");
        assert(report.can_proceed && "Should allow proceeding with no modifications");
        
        std::cout << "✓ Empty modification list test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running SafetyChecker unit tests..." << std::endl;
        
        testBasicSafetyChecks();
        testSuspiciousPatternDetection();
        testModificationSizeChecks();
        testCredentialDetection();
        testDependencyConflictDetection();
        testStrictMode();
        testBackupReadinessCheck();
        testSafetyLevelUtilities();
        testEmptyModificationList();
        
        std::cout << "All SafetyChecker tests passed!" << std::endl;
    }
};

int main() {
    try {
        SafetyCheckerTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All SafetyChecker component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}