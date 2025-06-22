# Camus `amodify` Command

The `amodify` (Auto-Modify) command is Camus's flagship feature for making intelligent, project-wide code modifications using Large Language Models (LLMs).

## Overview

`amodify` analyzes your entire codebase, understands the context and relationships between files, and then uses an LLM to generate comprehensive modifications across multiple files simultaneously. This enables complex refactoring, feature additions, and bug fixes that would be time-consuming to do manually.

## Key Features

- 🔍 **Intelligent File Discovery**: Automatically scans and prioritizes relevant files
- 🧠 **Context-Aware Analysis**: Builds comprehensive context within token limits
- 🛡️ **Safety-First Approach**: Multiple layers of validation and safety checks
- 💾 **Automatic Backups**: Creates backups before making any changes
- 🎯 **Interactive Confirmation**: Review and approve changes before application
- 📊 **Comprehensive Logging**: Detailed audit trails for all operations

## Basic Usage

```bash
# Basic project-wide modification
camus amodify "Add error handling to all database operations"

# Limit the scope to specific file types
camus amodify "Refactor authentication logic" --include "*.ts,*.js"

# Exclude certain directories
camus amodify "Update API endpoints" --exclude "node_modules/,build/"

# Set maximum number of files to process
camus amodify "Add JSDoc comments" --max-files 50
```

## Command Syntax

```bash
camus amodify <prompt> [options]
```

### Required Arguments

- `<prompt>`: High-level description of the changes you want to make

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `--max-files <n>` | Maximum number of files to process | 100 |
| `--max-tokens <n>` | Maximum tokens for LLM context | 128000 |
| `--include <patterns>` | Include only files matching patterns | (from config) |
| `--exclude <patterns>` | Exclude files matching patterns | (from config) |
| `--no-backup` | Skip backup creation | false |
| `--force` | Skip interactive confirmation | false |
| `--dry-run` | Show what would be done without making changes | false |

## Workflow Overview

The `amodify` command follows a comprehensive 7-step workflow:

### 1. File Discovery 📁
- Recursively scans the project directory
- Applies include/exclude patterns from configuration
- Filters by file extensions and size limits
- Respects `.gitignore` and custom ignore patterns

### 2. Context Building 🏗️
- Prioritizes files based on:
  - File type and importance
  - Modification recency (git-aware)
  - Relevance to the user's request
  - File size and complexity
- Builds comprehensive context within token limits
- Applies intelligent truncation when necessary

### 3. LLM Interaction 🤖
- Sends structured prompt with full project context
- Includes user request and all relevant file contents
- Receives structured response with file modifications
- Handles network errors and API limitations gracefully

### 4. Response Parsing 📝
- Extracts file modifications from LLM response
- Validates file paths and content format
- Detects new file creation vs. existing file modification
- Applies security validation and content filtering

### 5. Safety Validation 🛡️
- **Git Status Check**: Warns about uncommitted changes
- **File Permissions**: Ensures write access to target files
- **Size Validation**: Checks modification size limits
- **Pattern Detection**: Scans for suspicious or dangerous code
- **Dependency Analysis**: Alerts about critical file modifications
- **Disk Space**: Ensures sufficient space for changes and backups

### 6. Backup Creation 💾
- Creates timestamped backups of existing files
- Organizes backups in structured directory hierarchy
- Enables easy restoration if needed
- Tracks backup metadata for audit purposes

### 7. Interactive Review & Application ✅
- Displays comprehensive diffs for all modifications
- Interactive mode for reviewing each file individually
- Bulk approval for trusted operations
- Applies changes atomically where possible

## Configuration

The `amodify` command is configured via the `.camus/config.yml` file:

```yaml
# amodify-specific settings
amodify:
  max_files: 100                    # Default file limit
  max_tokens: 128000               # Default token limit for LLM context
  include_extensions:              # File types to include
    - '.cpp'
    - '.hpp'
    - '.h'
    - '.c'
    - '.py'
    - '.js'
    - '.ts'
  default_ignore_patterns:         # Patterns to ignore
    - '.git/'
    - 'build/'
    - 'node_modules/'
    - '__pycache__/'
    - '*.o'
    - '*.pyc'
  create_backups: true            # Enable backup creation
  backup_dir: '.camus/backups'    # Backup storage location
  interactive_threshold: 5        # Use interactive mode for >5 files
  git_check: true                 # Check for clean git working directory
```

## Safety Features

### Multi-Layer Validation

1. **Input Validation**: Validates user prompts and command arguments
2. **Path Validation**: Prevents path traversal and system file access
3. **Content Scanning**: Detects potentially dangerous code patterns
4. **Permission Checks**: Ensures proper file system permissions
5. **Size Limits**: Prevents excessive resource consumption

### Dangerous Pattern Detection

The safety system automatically detects and warns about:

- System commands (`rm -rf`, `format`, `del`)
- Database operations (`DROP DATABASE`, `TRUNCATE TABLE`)
- Code injection patterns (`eval()`, `exec()`)
- Hardcoded credentials and secrets
- File system manipulation commands

### Recovery Options

- **Automatic Backups**: Every modification is backed up automatically
- **Restoration Command**: Easy restoration from backups
- **Git Integration**: Works well with version control workflows
- **Atomic Operations**: Minimizes risk of partial failures

## Advanced Usage

### Working with Large Codebases

```bash
# Focus on specific directories
camus amodify "Add logging to API endpoints" --include "src/api/**/*.ts"

# Increase processing limits for large projects
camus amodify "Refactor imports" --max-files 200 --max-tokens 200000

# Use multiple passes for complex changes
camus amodify "Phase 1: Update interfaces" --include "**/*.d.ts"
camus amodify "Phase 2: Update implementations" --include "**/*.ts" --exclude "**/*.d.ts"
```

### Integration with Development Workflow

```bash
# Check git status first
git status

# Run amodify with automatic backup
camus amodify "Fix TypeScript strict mode errors"

# Review changes
git diff

# Commit if satisfied
git add -A
git commit -m "Fix TypeScript strict mode errors

🤖 Generated with Camus amodify"
```

### Dry Run Mode

```bash
# See what would be changed without making modifications
camus amodify "Add unit tests for utility functions" --dry-run

# Review the plan, then run for real
camus amodify "Add unit tests for utility functions"
```

## Output and Logging

### Console Output

The command provides real-time feedback:

```
Starting project-wide modification...
Request: Add error handling to all database operations

[1/7] Scanning project files...
Found 47 relevant files, limiting to 30

[2/7] Building context from 30 files...
Context built: 28 files, ~95,432 tokens

[3/7] Sending request to LLM... (this may take a while)
Request completed successfully

[4/7] Parsing LLM response...
Parsed 12 file modifications

[5/7] Performing safety checks...
Safety level: Safe

[6/7] Creating backups...
Created 12 backups in .camus/backups/20231222_143052/

[7/7] Review proposed changes...
```

### Comprehensive Logging

All operations are logged to `.camus/logs/` with:

- Session start/end times and duration
- File discovery statistics
- Context building metrics
- LLM interaction details (tokens, response time)
- Safety check results
- File modification outcomes
- Error details and stack traces

## Error Handling

### Common Issues and Solutions

#### "No relevant files found"
- Check include/exclude patterns in configuration
- Ensure you're in the correct directory
- Verify file extensions are in the include list

#### "Token limit exceeded"
- Reduce `--max-files` parameter
- Increase `--max-tokens` if using a larger model
- Use more specific include patterns to focus scope

#### "Safety checks failed"
- Review the safety report for specific issues
- Address git working directory status
- Check file permissions
- Use `--force` only if you're certain changes are safe

#### "LLM request failed"
- Check internet connection
- Verify API configuration in `.camus/config.yml`
- Check model availability and rate limits
- Review logs for detailed error information

### Backup and Recovery

If something goes wrong:

```bash
# List available backups
ls .camus/backups/

# Restore from specific backup session
camus restore --from .camus/backups/20231222_143052/

# Restore individual file
cp .camus/backups/20231222_143052/src_main.cpp src/main.cpp
```

## Best Practices

### 1. Start Small
Begin with focused requests on smaller codebases to understand how the system works.

### 2. Use Version Control
Always work with a clean git working directory and commit changes regularly.

### 3. Review Carefully
Take time to review generated changes, especially for critical code paths.

### 4. Iterative Approach
Break complex modifications into smaller, focused requests.

### 5. Test Thoroughly
Run your test suite after applying modifications to ensure functionality is preserved.

### 6. Backup Strategy
Keep backups enabled and periodically clean old backup directories.

## Troubleshooting

### Performance Optimization

```bash
# Profile token usage
camus amodify "..." --dry-run | grep "tokens"

# Optimize file selection
camus amodify "..." --include "src/**/*.cpp" --exclude "tests/"

# Reduce context size
camus amodify "..." --max-files 20
```

### Debugging Issues

```bash
# Enable verbose logging
export CAMUS_LOG_LEVEL=debug
camus amodify "..."

# Check log files
tail -f .camus/logs/camus_*.log

# Validate configuration
camus init --check
```

## Examples

### Adding Feature Across Multiple Files

```bash
camus amodify "Add comprehensive input validation to all API endpoints with proper error messages and status codes"
```

### Refactoring Code Structure

```bash
camus amodify "Refactor the authentication system to use dependency injection pattern"
```

### Updating Dependencies

```bash
camus amodify "Update all import statements to use the new @latest/api-client package instead of the deprecated api-client"
```

### Adding Documentation

```bash
camus amodify "Add JSDoc comments to all public methods and classes with proper parameter and return type documentation"
```

### Security Improvements

```bash
camus amodify "Add input sanitization and SQL injection prevention to all database query functions"
```

The `amodify` command represents the cutting edge of AI-assisted software development, enabling developers to make sweeping improvements to their codebases with confidence and safety.