# Camus - Refined Project-Level Context Implementation Plan

## Overview

This document outlines a detailed implementation plan for the `amodify` command - an advanced feature that enables project-wide code modifications through LLM analysis of the entire codebase context.

## High-Level Architecture

The `amodify` command will consist of four main components:
1. **ProjectScanner** - File discovery and filtering
2. **ContextBuilder** - Large prompt construction 
3. **ResponseParser** - Structured LLM output parsing
4. **MultiFileDiff** - Cross-file change visualization and confirmation

## Task Breakdown

### Phase 1: Core Infrastructure

#### Task 1.1: Implement ProjectScanner Class
**Files**: `include/Camus/ProjectScanner.hpp`, `src/Camus/ProjectScanner.cpp`

**Objective**: Create a robust file discovery system with intelligent filtering.

**Implementation Details**:
- Use `<filesystem>` library for recursive directory traversal
- Support `.camusignore` file with gitignore-style syntax
- Default ignore patterns: `.git/`, `build/`, `*.o`, `*.a`, `*.so`, `*.dll`, `node_modules/`, `__pycache__/`
- Default include extensions: `.cpp`, `.hpp`, `.h`, `.c`, `.py`, `.js`, `.ts`, `.md`, `.yml`, `.yaml`, `.json`
- Size limits: Skip files larger than 1MB to prevent context overflow
- Binary detection: Skip files containing null bytes

**Public Interface**:
```cpp
class ProjectScanner {
public:
    explicit ProjectScanner(const std::string& root_path);
    std::vector<std::string> scanFiles();
    void addIgnorePattern(const std::string& pattern);
    void addIncludeExtension(const std::string& extension);
    
private:
    bool shouldIgnoreFile(const std::string& relative_path) const;
    bool isTextFile(const std::string& file_path) const;
    void loadCamusIgnore();
};
```

#### Task 1.2: Implement IgnorePattern Matching
**Files**: `include/Camus/IgnorePattern.hpp`, `src/Camus/IgnorePattern.cpp`

**Objective**: Create gitignore-compatible pattern matching.

**Implementation Details**:
- Support wildcards (`*`, `**`, `?`)
- Support negation patterns (`!pattern`)
- Support directory-only patterns (`pattern/`)
- Case-sensitive matching by default
- Relative path normalization

#### Task 1.3: Extend CMakeLists.txt Dependencies
**Files**: `CMakeLists.txt`

**Objective**: Add required dependencies for filesystem operations.

**Changes**:
- Ensure C++17 support for `<filesystem>`
- Add any additional libraries if needed for pattern matching

### Phase 2: Context Management

#### Task 2.1: Implement ContextBuilder Class
**Files**: `include/Camus/ContextBuilder.hpp`, `src/Camus/ContextBuilder.cpp`

**Objective**: Efficiently construct large context prompts with smart truncation.

**Implementation Details**:
- Token counting approximation (4 chars ≈ 1 token)
- Context window management (default 128k tokens, configurable)
- File prioritization (recently modified files first)
- Smart truncation strategies (keep full small files, truncate large files)
- Clear file separators and metadata

**Public Interface**:
```cpp
class ContextBuilder {
public:
    explicit ContextBuilder(size_t max_tokens = 128000);
    std::string buildContext(const std::vector<std::string>& file_paths, 
                           const std::string& user_request);
    void setMaxTokens(size_t max_tokens);
    
private:
    size_t estimateTokens(const std::string& text) const;
    std::string formatFileContent(const std::string& path, const std::string& content) const;
    std::vector<std::string> prioritizeFiles(const std::vector<std::string>& files) const;
};
```

#### Task 2.2: Implement Smart File Prioritization
**Objective**: Prioritize files based on relevance and recency.

**Implementation Details**:
- Git modification timestamp priority
- File size consideration (smaller files prioritized)
- File type hierarchy (headers > source > config > docs)
- User-specified file patterns from command line

### Phase 3: Response Processing

#### Task 3.1: Implement ResponseParser Class
**Files**: `include/Camus/ResponseParser.hpp`, `src/Camus/ResponseParser.cpp`

**Objective**: Parse structured LLM responses into actionable file modifications.

**Implementation Details**:
- Parse `--- FILE: path ---` markers
- Extract complete file contents between markers
- Handle malformed responses gracefully
- Validate file paths exist in project
- Support both absolute and relative paths

**Public Interface**:
```cpp
struct FileModification {
    std::string file_path;
    std::string new_content;
    bool is_new_file = false;
};

class ResponseParser {
public:
    std::vector<FileModification> parseResponse(const std::string& llm_response);
    
private:
    bool isValidFilePath(const std::string& path) const;
    std::string normalizeFilePath(const std::string& path) const;
};
```

#### Task 3.2: Implement Response Validation
**Objective**: Validate parsed responses before applying changes.

**Implementation Details**:
- Syntax checking for common languages
- File path validation
- Content sanity checks (not empty, reasonable size)
- Backup creation before modification

### Phase 4: User Interface

#### Task 4.1: Implement MultiFileDiff Class
**Files**: `include/Camus/MultiFileDiff.hpp`, `src/Camus/MultiFileDiff.cpp`

**Objective**: Display comprehensive diffs across multiple files with user confirmation.

**Implementation Details**:
- Reuse existing dtl library for individual file diffs
- Color-coded output for additions/deletions/modifications
- File-by-file navigation options
- Summary statistics (X files changed, Y insertions, Z deletions)
- Batch confirmation for all changes

**Public Interface**:
```cpp
class MultiFileDiff {
public:
    bool showDiffsAndConfirm(const std::vector<FileModification>& modifications);
    
private:
    void displayFileDiff(const std::string& file_path, 
                        const std::string& original_content,
                        const std::string& new_content);
    void displaySummary(const std::vector<FileModification>& modifications);
};
```

#### Task 4.2: Enhanced User Confirmation Interface
**Objective**: Provide intuitive confirmation flow with preview options.

**Implementation Details**:
- File-by-file approval option: `[a]ll, [y]es, [n]o, [s]kip, [q]uit`
- Diff context lines configuration
- Preview mode before applying changes
- Undo functionality (backup restoration)

### Phase 5: Core Integration

#### Task 5.1: Implement Core::handleAmodify Method
**Files**: `src/Camus/Core.cpp`, `include/Camus/Core.hpp`

**Objective**: Integrate all components into the main command handler.

**Implementation Steps**:
1. Initialize ProjectScanner with current directory
2. Scan and filter relevant files
3. Build context prompt with ContextBuilder
4. Send prompt to LLM via existing interface
5. Parse response with ResponseParser
6. Display diffs with MultiFileDiff
7. Apply changes if confirmed

**Error Handling**:
- File access permission errors
- LLM context window overflow
- Malformed LLM responses
- Partial file write failures

#### Task 5.2: Add Command Line Integration
**Files**: `src/main.cpp`, command parsing logic

**Objective**: Add `amodify` command to CLI interface.

**Command Syntax**:
```bash
camus amodify "Add user authentication system"
camus amodify --max-files 50 "Refactor database layer"
camus amodify --include="src/**" "Add logging to all services"
```

**Options**:
- `--max-files N`: Limit number of files to include
- `--include PATTERN`: Include only files matching pattern
- `--exclude PATTERN`: Exclude files matching pattern
- `--max-tokens N`: Override default context window size

### Phase 6: Configuration and Polish

#### Task 6.1: Add Configuration Options
**Files**: `.camus/config.yml` template, `src/Camus/ConfigParser.cpp`

**New Configuration Keys**:
```yaml
# Project scanning settings
amodify:
  max_files: 100
  max_tokens: 128000
  include_extensions: ['.cpp', '.hpp', '.h', '.c', '.py', '.js', '.ts']
  default_ignore_patterns: ['.git/', 'build/', 'node_modules/']
  create_backups: true
  backup_dir: '.camus/backups'
```

#### Task 6.2: Implement Safety Features
**Objective**: Add safeguards against accidental data loss.

**Features**:
- Automatic backup creation before modifications
- Git working directory clean check
- Dry-run mode (`--dry-run` flag)
- Maximum modification size limits
- Confirmation for destructive operations

#### Task 6.3: Add Comprehensive Logging
**Files**: `include/Camus/Logger.hpp`, `src/Camus/Logger.cpp`

**Objective**: Provide detailed logging for debugging and audit trails.

**Log Events**:
- File discovery and filtering results
- Context building statistics
- LLM request/response metadata
- File modification operations
- Error conditions and recovery

### Phase 7: Testing and Documentation

#### Task 7.1: Unit Tests
**Files**: `tests/ProjectScannerTest.cpp`, `tests/ContextBuilderTest.cpp`, etc.

**Test Coverage**:
- ProjectScanner file filtering logic
- ContextBuilder token management
- ResponseParser edge cases
- MultiFileDiff display logic

#### Task 7.2: Integration Tests
**Files**: `tests/AmodifyIntegrationTest.cpp`

**Test Scenarios**:
- End-to-end amodify workflow
- Large project handling
- Error recovery scenarios
- Configuration variations

#### Task 7.3: Documentation Updates
**Files**: `README.md`, `management/user-guide.md`

**Documentation Sections**:
- `amodify` command usage examples
- Configuration options
- Best practices and limitations
- Troubleshooting guide

## Implementation Timeline

1. **Week 1**: Phase 1 (Core Infrastructure)
2. **Week 2**: Phase 2 (Context Management) 
3. **Week 3**: Phase 3 (Response Processing)
4. **Week 4**: Phase 4 (User Interface)
5. **Week 5**: Phase 5 (Core Integration)
6. **Week 6**: Phase 6 (Configuration and Polish)
7. **Week 7**: Phase 7 (Testing and Documentation)

## Risk Mitigation

- **Context Window Limits**: Implement smart truncation and file prioritization
- **LLM Response Quality**: Add response validation and fallback strategies  
- **File System Permissions**: Comprehensive error handling and user feedback
- **Performance**: Async file reading and processing for large projects
- **Data Safety**: Automatic backups and git integration checks

## Success Criteria

- [ ] Successfully processes projects with 100+ files
- [ ] Handles context windows up to 128k tokens efficiently
- [ ] Provides clear, actionable diffs for all modifications
- [ ] Maintains data integrity with backup/restore functionality
- [ ] Integrates seamlessly with existing Camus commands
- [ ] Comprehensive test coverage (>90%)
- [ ] Clear documentation and examples

## Future Enhancements

- **Incremental Context**: Only include changed files since last run
- **Interactive Mode**: Step-by-step file selection and modification
- **Template System**: Predefined modification templates
- **Plugin Architecture**: Custom file processors and analyzers
- **Multi-Model Support**: Compare responses from different LLMs