// =================================================================
// tests/TaskClassifierTest.cpp
// =================================================================
// Unit tests for TaskClassifier component.

#include "Camus/TaskClassifier.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

class TaskClassifierTest {
private:
    Camus::TaskClassifier classifier;
    
public:
    TaskClassifierTest() : classifier() {}
    
    void testBasicClassification() {
        std::cout << "Testing basic task classification..." << std::endl;
        
        // Test code generation prompts
        auto result1 = classifier.classify("Create a function that calculates factorial");
        assert(result1.primary_type == Camus::TaskType::CODE_GENERATION && "Should classify as CODE_GENERATION");
        assert(result1.confidence > 0.0 && "Should have confidence > 0");
        
        auto result2 = classifier.classify("Write a new class for user authentication");
        assert(result2.primary_type == Camus::TaskType::CODE_GENERATION && "Should classify as CODE_GENERATION");
        
        // Test code analysis prompts
        auto result3 = classifier.classify("Explain what this function does");
        assert(result3.primary_type == Camus::TaskType::CODE_ANALYSIS && "Should classify as CODE_ANALYSIS");
        
        auto result4 = classifier.classify("Analyze this code and tell me how it works");
        assert(result4.primary_type == Camus::TaskType::CODE_ANALYSIS && "Should classify as CODE_ANALYSIS");
        
        // Test refactoring prompts
        auto result5 = classifier.classify("Refactor this code to improve performance");
        assert(result5.primary_type == Camus::TaskType::REFACTORING && "Should classify as REFACTORING");
        
        auto result6 = classifier.classify("Clean up this function and make it more readable");
        assert(result6.primary_type == Camus::TaskType::REFACTORING && "Should classify as REFACTORING");
        
        std::cout << "✓ Basic classification test passed" << std::endl;
    }
    
    void testBugFixingClassification() {
        std::cout << "Testing bug fixing classification..." << std::endl;
        
        auto result1 = classifier.classify("Fix the bug in this function");
        assert(result1.primary_type == Camus::TaskType::BUG_FIXING && "Should classify as BUG_FIXING");
        
        auto result2 = classifier.classify("Debug this crash issue");
        assert(result2.primary_type == Camus::TaskType::BUG_FIXING && "Should classify as BUG_FIXING");
        
        auto result3 = classifier.classify("Solve the error in the authentication module");
        assert(result3.primary_type == Camus::TaskType::BUG_FIXING && "Should classify as BUG_FIXING");
        
        auto result4 = classifier.classify("This code is broken, please repair it");
        assert(result4.primary_type == Camus::TaskType::BUG_FIXING && "Should classify as BUG_FIXING");
        
        std::cout << "✓ Bug fixing classification test passed" << std::endl;
    }
    
    void testDocumentationClassification() {
        std::cout << "Testing documentation classification..." << std::endl;
        
        auto result1 = classifier.classify("Add comments to this function");
        assert(result1.primary_type == Camus::TaskType::DOCUMENTATION && "Should classify as DOCUMENTATION");
        
        auto result2 = classifier.classify("Write documentation for this class");
        assert(result2.primary_type == Camus::TaskType::DOCUMENTATION && "Should classify as DOCUMENTATION");
        
        auto result3 = classifier.classify("Document what this method does");
        assert(result3.primary_type == Camus::TaskType::DOCUMENTATION && "Should classify as DOCUMENTATION");
        
        auto result4 = classifier.classify("Annotate this code with proper comments");
        assert(result4.primary_type == Camus::TaskType::DOCUMENTATION && "Should classify as DOCUMENTATION");
        
        std::cout << "✓ Documentation classification test passed" << std::endl;
    }
    
    void testSecurityClassification() {
        std::cout << "Testing security classification..." << std::endl;
        
        auto result1 = classifier.classify("Review this code for security vulnerabilities");
        assert(result1.primary_type == Camus::TaskType::SECURITY_REVIEW && "Should classify as SECURITY_REVIEW");
        
        auto result2 = classifier.classify("Check for SQL injection vulnerabilities");
        assert(result2.primary_type == Camus::TaskType::SECURITY_REVIEW && "Should classify as SECURITY_REVIEW");
        
        auto result3 = classifier.classify("Sanitize user input in this function");
        assert(result3.primary_type == Camus::TaskType::SECURITY_REVIEW && "Should classify as SECURITY_REVIEW");
        
        auto result4 = classifier.classify("Validate authentication logic for security issues");
        assert(result4.primary_type == Camus::TaskType::SECURITY_REVIEW && "Should classify as SECURITY_REVIEW");
        
        std::cout << "✓ Security classification test passed" << std::endl;
    }
    
    void testSimpleQueryClassification() {
        std::cout << "Testing simple query classification..." << std::endl;
        
        auto result1 = classifier.classify("What does this variable do?");
        assert(result1.primary_type == Camus::TaskType::SIMPLE_QUERY && "Should classify as SIMPLE_QUERY");
        
        auto result2 = classifier.classify("How can I use this API?");
        assert(result2.primary_type == Camus::TaskType::SIMPLE_QUERY && "Should classify as SIMPLE_QUERY");
        
        auto result3 = classifier.classify("Quick question about this function");
        assert(result3.primary_type == Camus::TaskType::SIMPLE_QUERY && "Should classify as SIMPLE_QUERY");
        
        auto result4 = classifier.classify("Small modification to variable name");
        assert(result4.primary_type == Camus::TaskType::SIMPLE_QUERY && "Should classify as SIMPLE_QUERY");
        
        std::cout << "✓ Simple query classification test passed" << std::endl;
    }
    
    void testContextualClassification() {
        std::cout << "Testing contextual classification..." << std::endl;
        
        // Test with file paths that suggest specific task types
        std::vector<std::string> test_files = {"src/test/AuthTest.cpp", "src/main.cpp"};
        std::vector<std::string> test_extensions = {".test.cpp", ".cpp"};
        
        auto result1 = classifier.classifyWithContext("Fix this issue", test_files, test_extensions);
        assert(result1.primary_type == Camus::TaskType::BUG_FIXING && "Should classify as BUG_FIXING with test context");
        
        std::vector<std::string> doc_files = {"README.md", "docs/api.md"};
        std::vector<std::string> doc_extensions = {".md"};
        
        auto result2 = classifier.classifyWithContext("Update this content", doc_files, doc_extensions);
        assert(result2.primary_type == Camus::TaskType::DOCUMENTATION && "Should classify as DOCUMENTATION with doc context");
        
        std::vector<std::string> security_files = {"src/security/auth.cpp", "src/validation.cpp"};
        std::vector<std::string> security_extensions = {".cpp"};
        
        auto result3 = classifier.classifyWithContext("Review this code", security_files, security_extensions);
        // Should be either SECURITY_REVIEW or CODE_ANALYSIS (both are valid)
        assert((result3.primary_type == Camus::TaskType::SECURITY_REVIEW || 
                result3.primary_type == Camus::TaskType::CODE_ANALYSIS) && 
               "Should classify with security context");
        
        std::cout << "✓ Contextual classification test passed" << std::endl;
    }
    
    void testConfidenceScoring() {
        std::cout << "Testing confidence scoring..." << std::endl;
        
        // Test high confidence classification
        auto result1 = classifier.classify("Create a new function that calculates the factorial of a number");
        assert(result1.confidence > 0.5 && "Should have high confidence for clear prompt");
        
        // Test lower confidence for ambiguous prompt
        auto result2 = classifier.classify("Do something with this code");
        assert(result2.confidence < 0.8 && "Should have lower confidence for ambiguous prompt");
        
        // Test very ambiguous prompt
        auto result3 = classifier.classify("Help");
        assert(result3.primary_type == Camus::TaskType::UNKNOWN && "Should classify as UNKNOWN for very ambiguous prompt");
        
        std::cout << "✓ Confidence scoring test passed" << std::endl;
    }
    
    void testAlternativeClassifications() {
        std::cout << "Testing alternative classifications..." << std::endl;
        
        auto result = classifier.classify("Improve and document this function");
        
        // Should have primary classification and alternatives
        assert(result.primary_type != Camus::TaskType::UNKNOWN && "Should have primary classification");
        assert(!result.alternatives.empty() && "Should have alternative classifications");
        assert(result.alternatives.size() <= 3 && "Should not have more than 3 alternatives");
        
        // Alternatives should be different from primary
        for (auto alt : result.alternatives) {
            assert(alt != result.primary_type && "Alternatives should be different from primary");
        }
        
        std::cout << "✓ Alternative classifications test passed" << std::endl;
    }
    
    void testTaskTypeUtilities() {
        std::cout << "Testing task type utilities..." << std::endl;
        
        // Test task type name conversion
        assert(Camus::TaskClassifier::getTaskTypeName(Camus::TaskType::CODE_GENERATION) == "CODE_GENERATION");
        assert(Camus::TaskClassifier::getTaskTypeName(Camus::TaskType::BUG_FIXING) == "BUG_FIXING");
        
        // Test task type from name conversion
        assert(Camus::TaskClassifier::getTaskTypeFromName("CODE_GENERATION") == Camus::TaskType::CODE_GENERATION);
        assert(Camus::TaskClassifier::getTaskTypeFromName("bug_fixing") == Camus::TaskType::BUG_FIXING);
        assert(Camus::TaskClassifier::getTaskTypeFromName("invalid") == Camus::TaskType::UNKNOWN);
        
        // Test get all task types
        auto all_types = Camus::TaskClassifier::getAllTaskTypes();
        assert(!all_types.empty() && "Should return non-empty list of task types");
        assert(all_types.size() == 8 && "Should return all 8 task types");
        
        std::cout << "✓ Task type utilities test passed" << std::endl;
    }
    
    void testClassificationStats() {
        std::cout << "Testing classification statistics..." << std::endl;
        
        // Reset stats before testing
        classifier.resetStats();
        auto initial_stats = classifier.getClassificationStats();
        assert(initial_stats.empty() && "Stats should be empty after reset");
        
        // Perform some classifications
        classifier.classify("Create a function");
        classifier.classify("Fix this bug");
        classifier.classify("Add comments");
        
        auto stats = classifier.getClassificationStats();
        assert(!stats.empty() && "Should have statistics after classifications");
        
        size_t total_classifications = 0;
        for (const auto& [type, count] : stats) {
            total_classifications += count;
        }
        assert(total_classifications == 3 && "Should have 3 total classifications");
        
        std::cout << "✓ Classification statistics test passed" << std::endl;
    }
    
    void testConfigurationUpdates() {
        std::cout << "Testing configuration updates..." << std::endl;
        
        Camus::ClassificationConfig new_config;
        new_config.confidence_threshold = 0.9;
        new_config.enable_keyword_analysis = false;
        
        classifier.updateConfig(new_config);
        
        auto current_config = classifier.getConfig();
        assert(current_config.confidence_threshold == 0.9 && "Should update confidence threshold");
        assert(!current_config.enable_keyword_analysis && "Should disable keyword analysis");
        
        // Test classification with updated config
        auto result = classifier.classify("Create a function");
        // With keyword analysis disabled, might have different results
        
        std::cout << "✓ Configuration updates test passed" << std::endl;
    }
    
    void testEmptyAndInvalidInputs() {
        std::cout << "Testing empty and invalid inputs..." << std::endl;
        
        // Test empty prompt
        auto result1 = classifier.classify("");
        assert(result1.primary_type == Camus::TaskType::UNKNOWN && "Should handle empty prompt");
        assert(result1.confidence == 0.0 && "Should have zero confidence for empty prompt");
        
        // Test whitespace-only prompt
        auto result2 = classifier.classify("   \t\n  ");
        assert(result2.primary_type == Camus::TaskType::UNKNOWN && "Should handle whitespace-only prompt");
        
        // Test very short prompt
        auto result3 = classifier.classify("Hi");
        // Should either be SIMPLE_QUERY or UNKNOWN
        assert((result3.primary_type == Camus::TaskType::SIMPLE_QUERY || 
                result3.primary_type == Camus::TaskType::UNKNOWN) && 
               "Should handle very short prompt");
        
        std::cout << "✓ Empty and invalid inputs test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running TaskClassifier unit tests..." << std::endl;
        std::cout << "====================================" << std::endl << std::endl;
        
        testBasicClassification();
        std::cout << std::endl;
        
        testBugFixingClassification();
        std::cout << std::endl;
        
        testDocumentationClassification();
        std::cout << std::endl;
        
        testSecurityClassification();
        std::cout << std::endl;
        
        testSimpleQueryClassification();
        std::cout << std::endl;
        
        testContextualClassification();
        std::cout << std::endl;
        
        testConfidenceScoring();
        std::cout << std::endl;
        
        testAlternativeClassifications();
        std::cout << std::endl;
        
        testTaskTypeUtilities();
        std::cout << std::endl;
        
        testClassificationStats();
        std::cout << std::endl;
        
        testConfigurationUpdates();
        std::cout << std::endl;
        
        testEmptyAndInvalidInputs();
        std::cout << std::endl;
        
        std::cout << "All TaskClassifier tests passed!" << std::endl;
    }
};

int main() {
    try {
        TaskClassifierTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All TaskClassifier component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}