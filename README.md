# Camus CLI

A high-performance, locally-run command-line AI assistant for developers. Camus leverages open-source Large Language Models (LLMs) through llama.cpp to provide intelligent code modifications, build error analysis, and development workflow automation—all running directly on your machine.

## Overview

Camus is a C++-based CLI tool designed to bring AI-powered development assistance to your local environment. Unlike cloud-based solutions, Camus runs entirely on your machine, ensuring privacy, speed, and offline capability.

### Key Features

- **Local-First Architecture**: All processing happens on your machine using open-source LLMs
- **High Performance**: Written entirely in C++ for maximum speed and efficiency
- **Privacy-Focused**: Your code never leaves your machine
- **Flexible Model Support**: Compatible with various GGUF-format models through llama.cpp
- **Project-Wide Modifications**: AI-powered modifications across multiple files with the `amodify` command
- **Intelligent Code Operations**: Natural language code modifications, refactoring, and analysis
- **Build Integration**: Automatic error analysis and fix suggestions for build failures
- **Git Integration**: Smart commit message generation and version control operations
- **Safety-First Design**: Comprehensive validation, backups, and security checks
- **Comprehensive Testing**: Extensive unit and integration test coverage

## Installation

### Prerequisites

- C++17 compatible compiler
- CMake 3.14 or higher
- Git
- A GGUF-format LLM model (see [Recommended Models](#recommended-models))

### Building from Source

```bash
git clone https://github.com/yourusername/camus.git
cd camus
mkdir build && cd build
cmake ..
cmake --build .
```

## Commands

### `camus init`
Initializes Camus configuration in your project directory.

Creates a `.camus/` directory with a `config.yml` file containing project-specific settings such as model path, build commands, and test commands.

### `camus amodify` ⭐ **NEW**
**Project-wide AI modifications** - The flagship feature for intelligent, multi-file code changes.

```bash
camus amodify "Add error handling to all database operations"
camus amodify "Refactor authentication to use dependency injection" --max-files 50
camus amodify "Add JSDoc comments to all public methods" --include "src/**/*.ts"
```

Features:
- **Intelligent file discovery** and prioritization
- **Context-aware analysis** of your entire codebase
- **Safety-first approach** with multiple validation layers
- **Automatic backups** before making any changes
- **Interactive review** of all proposed modifications
- **Comprehensive logging** and audit trails

See [AmodifyCommand.md](docs/AmodifyCommand.md) for complete documentation.

### `camus modify`
Modifies individual source files based on natural language instructions.

```bash
camus modify "add a function that calculates factorial" --file src/math.cpp
```

- Reads the specified file
- Sends your instruction and code to the LLM
- Shows a diff preview of proposed changes
- Prompts for confirmation before applying

### `camus build`
Compiles your project with intelligent error handling.

```bash
camus build [cmake_args...]
```

If the build fails, Camus will:
- Capture compiler errors
- Analyze them using the LLM
- Suggest fixes for the errors

### `camus test`
Runs project tests with failure analysis.

```bash
camus test [test_args...]
```

On test failures, Camus provides:
- Analysis of failing tests
- Probable causes
- Suggested fixes

### `camus refactor`
Applies specific refactoring patterns to your code.

```bash
camus refactor "convert raw loops to STL algorithms" --file src/processor.cpp
```

Focused on specific transformations while preserving logic.

### `camus commit`
Generates conventional commit messages from staged changes.

```bash
camus commit
```

- Analyzes staged changes
- Generates descriptive commit message
- Allows editing before committing

### `camus push`
Simple wrapper for git push operations.

```bash
camus push
```

## Configuration

After running `camus init`, edit `.camus/config.yml` to configure:

- **model_path**: Path to your GGUF model file
- **default_model**: Model alias to use by default
- **build_command**: Your project's build command
- **test_command**: Your project's test command
- **amodify**: Advanced settings for project-wide modifications (file limits, token limits, safety settings)

Example configuration:
```yaml
# Backend configuration
backend: direct
model_path: /path/to/models/
default_model: Llama-3-8B-Instruct.Q4_K_M.gguf

# Project-wide modification settings
amodify:
  max_files: 100
  max_tokens: 128000
  include_extensions: ['.cpp', '.hpp', '.py', '.ts']
  create_backups: true
  interactive_threshold: 5
```

## Recommended Models

| Model | Size | Best For |
|-------|------|----------|
| **Llama 3 8B Instruct** | 8B | Default choice. Excellent for most tasks on consumer hardware |
| **DeepSeek Coder 6.7B** | 6.7B | Specialized for code generation and understanding |
| **Mistral 7B Instruct** | 7B | Quick, simple tasks with minimal resource usage |
| **Mixtral 8x7B** | ~47B | Complex refactoring on high-end hardware |
| **Code Llama 13B** | 13B | Balanced option for all commands |

All models should be in GGUF format, compatible with llama.cpp. Download from platforms like Hugging Face.

## Architecture

Camus follows a modular architecture:

- **Core**: Main application orchestrator
- **CliParser**: Command-line interface using CLI11
- **LlmInteraction**: Interface to llama.cpp for model inference
- **SysInteraction**: File system and process management

## Testing

Camus includes a comprehensive testing framework with both unit and integration tests:

```bash
# Run all tests
./build/tests/TestRunner

# Run specific test suites
./build/tests/TestRunner --test ProjectScanner
./build/tests/TestRunner --test Integration

# Individual test executables
./build/tests/ProjectScannerTest
./build/tests/ContextBuilderTest
./build/tests/ResponseParserTest
./build/tests/SafetyCheckerTest
./build/tests/IntegrationTest
```

Test coverage includes:
- File discovery and filtering logic
- Context building and token management
- LLM response parsing and validation
- Safety checks and security validation
- Backup and restore functionality
- End-to-end workflow integration

See [Testing.md](docs/Testing.md) for detailed testing documentation.

## Project Status

🎉 **Version 1.0 - Feature Complete!**

✅ **Completed:**
- Core CLI framework with complete command set
- Project-wide modifications with `amodify` command
- Intelligent file discovery and context building
- LLM response parsing and validation
- Comprehensive safety and security systems
- Automatic backup and restore functionality
- Configuration management system
- Complete logging and audit trails
- Extensive unit and integration test coverage
- Full documentation and examples

✅ **Architecture Components:**
- Core application orchestrator
- Advanced CLI parsing with CLI11
- LLM integration (llama.cpp and Ollama)
- File system and process management
- Multi-file diff visualization
- Interactive confirmation system
- YAML configuration parsing
- Enterprise-grade logging system

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

[Your chosen license]

## Acknowledgments

- Built on top of [llama.cpp](https://github.com/ggerganov/llama.cpp)
- CLI powered by [CLI11](https://github.com/CLIUtils/CLI11)
