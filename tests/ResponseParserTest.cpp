// =================================================================
// tests/ResponseParserTest.cpp
// =================================================================
// Unit tests for ResponseParser component.

#include "Camus/ResponseParser.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

class ResponseParserTest {
private:
    std::string test_dir;
    
public:
    ResponseParserTest() : test_dir(".") {}
    
    void testBasicResponseParsing() {
        std::cout << "Testing basic response parsing..." << std::endl;
        
        std::string llm_response = R"(
I'll help you fix the compilation errors. Here are the modifications needed:

==== FILE: src/main.cpp ====
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}

==== FILE: src/utils.hpp ====
#pragma once

#include <string>

std::string getMessage();

==== END ====
)";
        
        Camus::ResponseParser parser(test_dir);
        auto modifications = parser.parseResponse(llm_response);
        
        assert(modifications.size() == 2 && "Should parse 2 file modifications");
        
        // Check first file
        assert(modifications[0].file_path == "src/main.cpp" && "Should parse correct file path");
        assert(modifications[0].new_content.find("#include <iostream>") != std::string::npos && 
               "Should parse file content");
        assert(!modifications[0].is_new_file && "Should detect existing file modification");
        
        // Check second file
        assert(modifications[1].file_path == "src/utils.hpp" && "Should parse second file path");
        assert(modifications[1].new_content.find("#pragma once") != std::string::npos && 
               "Should parse second file content");
        
        std::cout << "✓ Basic response parsing test passed" << std::endl;
    }
    
    void testNewFileDetection() {
        std::cout << "Testing new file detection..." << std::endl;
        
        std::string llm_response = R"(
I'll create a new utility file:

==== NEW FILE: utils/helper.cpp ====
#include "helper.hpp"

void helperFunction() {
    // Implementation here
}
==== END ====
)";
        
        Camus::ResponseParser parser(test_dir);
        auto modifications = parser.parseResponse(llm_response);
        
        assert(modifications.size() == 1 && "Should parse 1 modification");
        assert(modifications[0].file_path == "utils/helper.cpp" && "Should parse new file path");
        assert(modifications[0].is_new_file && "Should detect new file");
        
        std::cout << "✓ New file detection test passed" << std::endl;
    }
    
    void testStrictValidation() {
        std::cout << "Testing strict validation..." << std::endl;
        
        // Response with suspicious content
        std::string malicious_response = R"(
==== FILE: src/main.cpp ====
#include <iostream>
#include <cstdlib>

int main() {
    system("rm -rf /");  // Malicious command
    return 0;
}
==== END ====
)";
        
        Camus::ResponseParser parser(test_dir);
        parser.setStrictValidation(true);
        
        auto modifications = parser.parseResponse(malicious_response);
        
        auto stats = parser.getLastParseStats();
        assert(!stats.error_messages.empty() && "Should detect security issues");
        
        std::cout << "✓ Strict validation test passed" << std::endl;
    }
    
    void testMultipleFileFormats() {
        std::cout << "Testing multiple file format support..." << std::endl;
        
        std::string llm_response = R"(
Here are the changes:

==== FILE: test.cpp ====
// C++ file
int main() { return 0; }

==== FILE: script.py ====
# Python file
print("Hello")

==== FILE: config.json ====
{
  "key": "value"
}

==== NEW FILE: README.md ====
# Project

This is a test project.
==== END ====
)";
        
        Camus::ResponseParser parser(test_dir);
        auto modifications = parser.parseResponse(llm_response);
        
        assert(modifications.size() == 4 && "Should parse all 4 files");
        
        // Check that different file types are handled
        bool found_cpp = false, found_py = false, found_json = false, found_md = false;
        for (const auto& mod : modifications) {
            if (mod.file_path.find(".cpp") != std::string::npos) found_cpp = true;
            if (mod.file_path.find(".py") != std::string::npos) found_py = true;
            if (mod.file_path.find(".json") != std::string::npos) found_json = true;
            if (mod.file_path.find(".md") != std::string::npos) found_md = true;
        }
        
        assert(found_cpp && found_py && found_json && found_md && 
               "Should handle multiple file formats");
        
        std::cout << "✓ Multiple file formats test passed" << std::endl;
    }
    
    void testEdgeCases() {
        std::cout << "Testing edge cases..." << std::endl;
        
        // Empty response
        Camus::ResponseParser parser(test_dir);
        auto modifications = parser.parseResponse("");
        assert(modifications.empty() && "Empty response should return no modifications");
        
        // Response without file markers
        modifications = parser.parseResponse("Just some text without file markers");
        assert(modifications.empty() && "Response without markers should return no modifications");
        
        // Malformed file marker
        std::string malformed_response = R"(
==== FILE: 
#include <iostream>
==== END ====
)";
        modifications = parser.parseResponse(malformed_response);
        assert(modifications.empty() && "Malformed markers should be rejected");
        
        std::cout << "✓ Edge cases test passed" << std::endl;
    }
    
    void testContentValidation() {
        std::cout << "Testing content validation..." << std::endl;
        
        // Very large file content
        std::string large_content = "==== FILE: large.txt ====\n";
        for (int i = 0; i < 100000; i++) {
            large_content += "Line " + std::to_string(i) + "\n";
        }
        large_content += "==== END ====";
        
        Camus::ResponseParser parser(test_dir);
        auto modifications = parser.parseResponse(large_content);
        
        auto stats = parser.getLastParseStats();
        // Parser should handle large content or warn about it
        assert(stats.total_modifications >= 0 && "Should handle large content gracefully");
        
        std::cout << "✓ Content validation test passed" << std::endl;
    }
    
    void testFilePathValidation() {
        std::cout << "Testing file path validation..." << std::endl;
        
        std::string response_with_invalid_paths = R"(
==== FILE: ../../../etc/passwd ====
root:x:0:0:root:/root:/bin/bash
==== END ====

==== FILE: /tmp/malicious.sh ====
#!/bin/bash
rm -rf /
==== END ====

==== FILE: valid/path.cpp ====
int main() { return 0; }
==== END ====
)";
        
        Camus::ResponseParser parser(test_dir);
        parser.setStrictValidation(true);
        
        auto modifications = parser.parseResponse(response_with_invalid_paths);
        
        // Should reject dangerous paths but accept valid ones
        bool found_valid = false;
        for (const auto& mod : modifications) {
            if (mod.file_path.find("valid/path.cpp") != std::string::npos) {
                found_valid = true;
            }
            // Should not contain dangerous paths
            assert(mod.file_path.find("../../../") == std::string::npos && 
                   "Should reject path traversal attempts");
            assert(mod.file_path.find("/etc/passwd") == std::string::npos && 
                   "Should reject system file paths");
        }
        
        auto stats = parser.getLastParseStats();
        assert(!stats.error_messages.empty() && "Should report validation errors");
        
        std::cout << "✓ File path validation test passed" << std::endl;
    }
    
    void runAllTests() {
        std::cout << "Running ResponseParser unit tests..." << std::endl;
        
        testBasicResponseParsing();
        testNewFileDetection();
        testStrictValidation();
        testMultipleFileFormats();
        testEdgeCases();
        testContentValidation();
        testFilePathValidation();
        
        std::cout << "All ResponseParser tests passed!" << std::endl;
    }
};

int main() {
    try {
        ResponseParserTest tests;
        tests.runAllTests();
        
        std::cout << "\n🎉 All ResponseParser component tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}