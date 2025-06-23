// =================================================================
// tests/TestRunner.cpp
// =================================================================
// Main test runner for all unit tests.

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <cstdlib>

// Test function declarations
int runProjectScannerTests();
int runContextBuilderTests();
int runResponseParserTests();
int runSafetyCheckerTests();
int runTaskClassifierTests();
int runModelAbstractionTests();
int runModelRegistryTests();
int runModelSelectorTests();
int runLoadBalancerTests();
int runIntegrationTests();

struct TestSuite {
    std::string name;
    std::function<int()> test_function;
};

class TestRunner {
private:
    std::vector<TestSuite> test_suites;
    
public:
    TestRunner() {
        // Register all test suites
        test_suites = {
            {"ProjectScanner", runProjectScannerTests},
            {"ContextBuilder", runContextBuilderTests},
            {"ResponseParser", runResponseParserTests},
            {"SafetyChecker", runSafetyCheckerTests},
            {"TaskClassifier", runTaskClassifierTests},
            {"ModelAbstraction", runModelAbstractionTests},
            {"ModelRegistry", runModelRegistryTests},
            {"ModelSelector", runModelSelectorTests},
            {"LoadBalancer", runLoadBalancerTests},
            {"Integration", runIntegrationTests}
        };
    }
    
    int runAllTests() {
        std::cout << "🚀 Starting Camus Unit Test Suite" << std::endl;
        std::cout << "=================================" << std::endl << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        int total_tests = test_suites.size();
        int passed_tests = 0;
        int failed_tests = 0;
        
        for (const auto& suite : test_suites) {
            std::cout << "Running " << suite.name << " tests..." << std::endl;
            std::cout << std::string(40, '-') << std::endl;
            
            auto suite_start = std::chrono::steady_clock::now();
            
            try {
                int result = suite.test_function();
                auto suite_end = std::chrono::steady_clock::now();
                auto suite_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    suite_end - suite_start);
                
                if (result == 0) {
                    std::cout << "✅ " << suite.name << " tests PASSED";
                    std::cout << " (took " << suite_duration.count() << "ms)" << std::endl;
                    passed_tests++;
                } else {
                    std::cout << "❌ " << suite.name << " tests FAILED";
                    std::cout << " (exit code: " << result << ")" << std::endl;
                    failed_tests++;
                }
            } catch (const std::exception& e) {
                auto suite_end = std::chrono::steady_clock::now();
                auto suite_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    suite_end - suite_start);
                
                std::cout << "❌ " << suite.name << " tests FAILED with exception: " 
                          << e.what() << std::endl;
                std::cout << " (took " << suite_duration.count() << "ms)" << std::endl;
                failed_tests++;
            }
            
            std::cout << std::endl;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // Print summary
        std::cout << "Test Summary" << std::endl;
        std::cout << "============" << std::endl;
        std::cout << "Total test suites: " << total_tests << std::endl;
        std::cout << "Passed: " << passed_tests << std::endl;
        std::cout << "Failed: " << failed_tests << std::endl;
        std::cout << "Total time: " << total_duration.count() << "ms" << std::endl;
        
        if (failed_tests == 0) {
            std::cout << std::endl << "🎉 All tests passed!" << std::endl;
            return 0;
        } else {
            std::cout << std::endl << "💥 " << failed_tests << " test suite(s) failed!" << std::endl;
            return 1;
        }
    }
    
    int runSpecificTest(const std::string& test_name) {
        for (const auto& suite : test_suites) {
            if (suite.name == test_name) {
                std::cout << "Running " << test_name << " tests only..." << std::endl;
                return suite.test_function();
            }
        }
        
        std::cerr << "Test suite '" << test_name << "' not found!" << std::endl;
        std::cerr << "Available test suites:" << std::endl;
        for (const auto& suite : test_suites) {
            std::cerr << "  - " << suite.name << std::endl;
        }
        return 1;
    }
    
    void listTests() {
        std::cout << "Available test suites:" << std::endl;
        for (const auto& suite : test_suites) {
            std::cout << "  - " << suite.name << std::endl;
        }
    }
};

// Test function implementations using system calls
int runProjectScannerTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/ProjectScannerTest");
    return result;
}

int runContextBuilderTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/ContextBuilderTest");
    return result;
}

int runResponseParserTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/ResponseParserTest");
    return result;
}

int runSafetyCheckerTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/SafetyCheckerTest");
    return result;
}

int runTaskClassifierTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/TaskClassifierTest");
    return result;
}

int runModelAbstractionTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/ModelAbstractionTest");
    return result;
}

int runModelRegistryTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/ModelRegistryTest");
    return result;
}

int runModelSelectorTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/ModelSelectorTest");
    return result;
}

int runLoadBalancerTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/LoadBalancerTest");
    return result;
}

int runIntegrationTests() {
    int result = std::system("cd /Users/genereux/Dev/camus && ./build/tests/IntegrationTest");
    return result;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help, -h          Show this help message" << std::endl;
    std::cout << "  --list, -l          List available test suites" << std::endl;
    std::cout << "  --test <name>       Run specific test suite" << std::endl;
    std::cout << "  (no arguments)      Run all test suites" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << "                    # Run all tests" << std::endl;
    std::cout << "  " << program_name << " --test ProjectScanner   # Run only ProjectScanner tests" << std::endl;
    std::cout << "  " << program_name << " --list                  # List available test suites" << std::endl;
}

int main(int argc, char* argv[]) {
    TestRunner runner;
    
    if (argc == 1) {
        // No arguments - run all tests
        return runner.runAllTests();
    }
    
    std::string arg1 = argv[1];
    
    if (arg1 == "--help" || arg1 == "-h") {
        printUsage(argv[0]);
        return 0;
    }
    
    if (arg1 == "--list" || arg1 == "-l") {
        runner.listTests();
        return 0;
    }
    
    if (arg1 == "--test" && argc == 3) {
        return runner.runSpecificTest(argv[2]);
    }
    
    std::cerr << "Invalid arguments!" << std::endl;
    printUsage(argv[0]);
    return 1;
}