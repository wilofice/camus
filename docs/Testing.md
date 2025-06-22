# Camus Testing Framework

This document describes the comprehensive testing framework for the Camus CLI application.

## Overview

The Camus testing framework consists of two main levels:

1. **Unit Tests** - Test individual components in isolation
2. **Integration Tests** - Test component interactions and complete workflows

## Test Structure

```
tests/
├── CMakeLists.txt           # Test build configuration
├── TestRunner.cpp           # Main test runner
├── ProjectScannerTest.cpp   # Unit tests for file scanning
├── ContextBuilderTest.cpp   # Unit tests for context building
├── ResponseParserTest.cpp   # Unit tests for LLM response parsing
├── SafetyCheckerTest.cpp    # Unit tests for safety validation
└── IntegrationTest.cpp      # End-to-end workflow tests
```

## Building Tests

Tests are automatically built as part of the main CMake configuration:

```bash
# Configure and build the project with tests
cmake -S . -B build
cmake --build build -j8

# The following test executables will be created in build/tests/:
# - ProjectScannerTest
# - ContextBuilderTest  
# - ResponseParserTest
# - SafetyCheckerTest
# - IntegrationTest
# - TestRunner
```

## Running Tests

### Run All Tests

```bash
# Run all tests using the test runner
./build/tests/TestRunner

# Or use CMake's built-in test runner
cd build && ctest
```

### Run Specific Test Suites

```bash
# Run individual test executables
./build/tests/ProjectScannerTest
./build/tests/ContextBuilderTest
./build/tests/ResponseParserTest
./build/tests/SafetyCheckerTest
./build/tests/IntegrationTest

# Or run specific suites via TestRunner
./build/tests/TestRunner --test ProjectScanner
./build/tests/TestRunner --test Integration
```

### List Available Tests

```bash
./build/tests/TestRunner --list
```

## Unit Test Details

### ProjectScannerTest
Tests the file discovery and filtering system:

- **Basic scanning**: Verifies recursive file discovery
- **Extension filtering**: Tests include/exclude by file extension
- **Ignore patterns**: Tests gitignore-style pattern matching  
- **File size limits**: Verifies large file exclusion
- **Empty directories**: Tests edge case handling

**Key test scenarios:**
```cpp
// Test file discovery with various filters
scanner.addIncludeExtension(".cpp");
scanner.addIgnorePattern("build/");
auto files = scanner.scanFiles();
```

### ContextBuilderTest
Tests the LLM context construction with token management:

- **Token limit enforcement**: Ensures contexts stay within limits
- **File prioritization**: Tests smart file ordering
- **Relevance scoring**: Tests keyword-based file prioritization
- **Truncation handling**: Tests graceful content truncation

**Key test scenarios:**
```cpp
// Test token limit respect
ContextBuilder builder(1000);  // Very small limit
auto stats = builder.getLastBuildStats();
assert(stats["tokens_used"] <= 1000);
```

### ResponseParserTest  
Tests LLM response parsing and validation:

- **File marker parsing**: Tests extraction of file modifications
- **New file detection**: Tests NEW FILE vs existing file logic
- **Content validation**: Tests security and format validation
- **Edge case handling**: Tests malformed response handling

**Key test scenarios:**
```cpp
// Test file modification parsing
std::string response = R"(
==== FILE: src/main.cpp ====
int main() { return 0; }
==== END ====
)";
auto modifications = parser.parseResponse(response);
```

### SafetyCheckerTest
Tests comprehensive safety validation:

- **Suspicious pattern detection**: Tests malicious code detection
- **File permission checks**: Tests write access validation
- **Size limit enforcement**: Tests modification size limits
- **Dependency conflict detection**: Tests critical file modification alerts

**Key test scenarios:**
```cpp
// Test dangerous pattern detection
modifications = {createModification("bad.cpp", "system(\"rm -rf /\")")};
auto report = checker.performSafetyChecks(modifications, config);
assert(report.overall_level >= SafetyLevel::DANGER);
```

## Integration Test Details

### Complete Workflow Test
Tests the full end-to-end amodify workflow:

1. **File scanning** with realistic project structure
2. **Context building** from discovered files
3. **Response parsing** with simulated LLM output
4. **Safety validation** of proposed changes
5. **Backup creation** for existing files

### Error Handling Test
Tests system behavior with malicious or malformed inputs:

- Malicious LLM responses with dangerous commands
- Security pattern detection and blocking
- Graceful degradation with invalid input

### Configuration Integration Test
Tests how different configuration settings affect the workflow:

- File count limits
- Token count limits  
- Extension filtering
- Ignore pattern application

### Backup and Restore Test
Tests the backup system functionality:

- Backup creation for existing files
- File restoration from backups
- Content integrity verification

## Test Assertions and Validation

### Standard Assertions
All tests use standard C++ assertions for validation:

```cpp
assert(condition && "Error message");
```

### Expected Behaviors
Tests validate:

- **Correctness**: Functions produce expected outputs
- **Error handling**: Invalid inputs are rejected gracefully
- **Performance**: Operations complete within reasonable time
- **Security**: Dangerous patterns are detected and blocked

## Test Data Management

### Temporary Files
Tests create temporary files and directories that are automatically cleaned up:

```cpp
void cleanupTestFiles() {
    if (std::filesystem::exists(test_dir)) {
        std::filesystem::remove_all(test_dir);
    }
}
```

### Realistic Test Data
Tests use realistic code samples and project structures to ensure real-world applicability.

## Continuous Integration

### CMake Integration
Tests are integrated with CMake's CTest framework:

```bash
# Run tests via CTest
cd build
ctest --output-on-failure

# Run tests in parallel
ctest -j8
```

### Test Properties
Tests are configured with appropriate timeouts and dependencies in CMakeLists.txt.

## Adding New Tests

### Creating Unit Tests
1. Create new test file in `tests/` directory
2. Add to `TEST_EXECUTABLES` list in `tests/CMakeLists.txt`
3. Add executable configuration and CTest integration
4. Update TestRunner to include new test suite

### Test Best Practices
1. **Isolation**: Each test should be independent
2. **Cleanup**: Always clean up temporary files/directories
3. **Assertions**: Use descriptive assertion messages
4. **Coverage**: Test both success and failure paths
5. **Realistic data**: Use representative test data

## Debugging Tests

### Debug Information
Tests output detailed logging information to help with debugging:

```
[INFO] Building context from 4 files...
[INFO] Available tokens for file content: 5000
[INFO] Context built: 4 files, ~563 tokens
```

### Test Failures
When tests fail, they provide specific error messages and context about what went wrong.

### Running Individual Components
Each test can be run individually for focused debugging:

```bash
# Debug specific component
./build/tests/ResponseParserTest
```

## Performance Considerations

### Test Execution Time
- Unit tests: < 1 second each
- Integration tests: < 5 seconds
- Full test suite: < 10 seconds

### Resource Usage
Tests are designed to be lightweight and not require external dependencies beyond the core Camus libraries.

## Coverage

The test suite provides comprehensive coverage of:

- ✅ File discovery and filtering
- ✅ Context building and token management  
- ✅ LLM response parsing and validation
- ✅ Safety checks and security validation
- ✅ Backup and restore functionality
- ✅ Configuration management
- ✅ Error handling and edge cases
- ✅ Component integration

This ensures the reliability and robustness of the Camus amodify functionality.