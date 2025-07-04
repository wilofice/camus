Camus - Multi-Model Pipeline Implementation Plan
1. Objective
To implement a "pipeline" feature for the modify command, allowing a sequence of different models to be called in order. The output of the first model will be used as the input code for the second model, and so on, enabling progressive refinement of the generated code.

2. High-Level Strategy
This feature will be built on top of our existing Ollama integration. The core changes will be in the configuration structure and the main command handler (Core.cpp), which will need to orchestrate the sequential calls.

Phase 1: Evolving the Configuration
The current configuration is too simple. We need a way to define individual models and then group them into named pipelines.

Task 1.1: Redesign the .camus/config.yml Structure
Objective: Update the configuration file format to support definitions for multiple models and pipelines.

File to Modify: src/Camus/Core.cpp (to update the defaultConfig string generated by the init command).

Implementation Guidelines:

The new structure will have three top-level keys: backends, pipelines, and defaults.

backends will be a list of named connection configurations.

pipelines will be a map where the key is the pipeline name and the value is a list of backend names.

New config.yml Example:

# Camus Configuration v2.0

# Define all available model backends here
backends:
  - name: deepseek-coder-local
    type: ollama
    url: http://localhost:11434
    model: deepseek-coder:6.7b

  - name: llama3-local
    type: ollama
    url: http://localhost:11434
    model: llama3:8b

  - name: direct-llama-cpp # Example of a future direct integration
    type: direct
    path: /path/to/models/my-model.gguf

# Define named sequences of backends
pipelines:
  # The default pipeline used by 'camus modify' without a flag
  default:
    - llama3-local

  # A custom pipeline for more complex refinement
  refine-pro:
    - llama3-local
    - deepseek-coder-local

# Set the default pipeline to use
defaults:
  pipeline: default

Task 1.2: Upgrade the ConfigParser
Objective: Enhance the ConfigParser to read and understand the new nested YAML structure.

Files to Modify: include/Camus/ConfigParser.hpp, src/Camus/ConfigParser.cpp.

Implementation Guidelines:

This will require switching from our simple manual parser to a proper, lightweight, header-only YAML parsing library. A great candidate is jbeder/yaml-cpp.

Add yaml-cpp as a new dependency in CMakeLists.txt using FetchContent.

The ConfigParser will now use this library to load the entire YAML file into a node structure.

Create new public methods like getBackendConfig(const std::string& name) and getPipeline(const std::string& name).

Testing / Verification: Write small test cases to ensure the parser can correctly read backend details and lists of models from a pipeline.

Phase 2: Implementing the Pipeline Logic
Task 2.1: Add a --pipeline Option to the modify Command
Objective: Allow the user to specify which pipeline to use from the command line.

File to Modify: src/Camus/CliParser.cpp.

Implementation Guidelines:

In setupModifyCommand, add a new optional flag: sub->add_option("--pipeline", m_commands.pipeline_name, "The name of the pipeline to use.");.

Add std::string pipeline_name; to the Commands struct in CliParser.hpp.

Testing / Verification: Run camus modify --help and verify the new option appears in the help text.

Task 2.2: Implement the Pipeline Execution Loop in Core
Objective: Orchestrate the sequential execution of models in a pipeline.

File to Modify: src/Camus/Core.cpp.

Implementation Guidelines:

Inside handleModify, determine the pipeline to use: if the user provided --pipeline, use that; otherwise, get the default pipeline name from the config.

Use the ConfigParser to get the list of backend names for the chosen pipeline.

Initialize a string current_code = original_code;.

Loop through the list of backend names. In each iteration:

Print a status message to the user: [INFO] Pipeline Step 1/3: Using model 'llama3-local'....

Get the config for the current backend (URL, model name, etc.).

Instantiate the appropriate LlmInteraction object (OllamaInteraction for now).

Construct the prompt using current_code as the code to be modified.

Call getCompletion() and store the result back into current_code. The output of one step becomes the input for the next.

After the loop finishes, the final current_code is the fully refined result.

Testing / Verification: Run a modify command with a two-step pipeline. Verify from the console output that both models are called in sequence.

Task 2.3: Display the Final Diff
Objective: Show the user the final, net change after the entire pipeline has run.

File to Modify: src/Camus/Core.cpp.

Implementation Guidelines:

The diff logic remains mostly the same. The key is to compare the original_code (from before the loop started) with the final current_code (from after the loop finished). This ensures the user sees the total effect of the pipeline, not the intermediate steps.

Testing / Verification: The diff displayed should accurately reflect the difference between the starting state of the file and the final output of the last model in the chain.