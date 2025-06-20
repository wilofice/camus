Camus CLI - Core Feature Implementation Plan
This document outlines the detailed plan for implementing the next set of core features for the Camus CLI.

1. Interactive modify Command
Objective
To transform the modify command from a simple print-out to a fully interactive workflow. The user will be shown a color-coded comparison of the changes (a "diff") and asked to confirm before any files are modified on disk.

Component Breakdown & Logic
New Component: DiffGenerator Class

Description: A new utility class responsible for comparing two strings of text (the original code and the LLM's proposed code) and generating a structured representation of the differences.

Logic: It will implement a line-by-line comparison. For each line, it will determine if the line is unchanged, an addition, or a deletion.

Files to Create: include/Camus/DiffGenerator.hpp and src/Camus/DiffGenerator.cpp.

Updated Component: Core::handleModify() Method

Description: This method's logic will be extended to orchestrate the new interactive workflow.

Step-by-step Logic:

The method will still begin by reading the file content and getting the modified_code from the LlmInteraction class as it does now.

It will then instantiate our new DiffGenerator: auto differ = DiffGenerator(original_code, modified_code);

It will call a method on the differ, like auto diff_lines = differ.getDiff();, to get the list of changes.

It will loop through diff_lines, printing each line to the console using ANSI escape codes for color:

Lines marked as ADDED will be printed in green.

Lines marked as REMOVED will be printed in red.

Lines marked as UNCHANGED will be printed in the default color.

After displaying the diff, it will print a confirmation prompt: Apply changes? [y]es, [n]o: .

It will wait for and read user input from the console (std::cin).

If the user enters 'y' or 'Y', it will call m_sys->writeFile() to save the modified_code to the original file path and print a "Changes applied." message.

If the user enters anything else, it will print a "Changes discarded." message and do nothing.

Input / Output Details
DiffGenerator Class:

Input: Two std::string arguments to its constructor: original_code and new_code.

Output: A std::vector of struct DiffLine, where DiffLine contains a std::string for the line text and an enum for its status (e.g., ADDED, REMOVED, UNCHANGED).

Core::handleModify() Method:

Input: The Commands struct from the CliParser.

Output (to console):

A color-coded diff view of the proposed changes.

The confirmation prompt text.

A final status message confirming the action taken.

Output (to file system): Potentially, an updated source file if the user confirms the changes.

2. LLM-Powered commit Command
Objective
To implement the commit command, which uses the LLM to automatically generate a descriptive commit message based on the currently staged changes in Git.

Component Breakdown & Logic
Updated Component: SysInteraction::executeCommand() Method

Description: This method needs to be fully implemented to be a robust, platform-agnostic way of running external shell commands.

Logic: It will use the standard C function popen (or _popen on Windows) to execute a command and open a pipe to its standard output. It will read the entire output from the pipe into a string and capture the command's exit code when the pipe is closed.

Updated Component: Core::handleCommit() Method

Description: This method will orchestrate the process of getting the diff, querying the LLM, and committing.

Step-by-step Logic:

Call m_sys->executeCommand("git", {"diff", "--staged"}).

Check the result. If the output string is empty, print "No changes staged to commit." and exit successfully.

If there is output, construct a detailed prompt for the LLM, e.g., "Generate a concise, conventional commit message for the following code changes:\n\n" + diff_output.

Call m_llm->getCompletion() to get the suggested commit message.

Display the message and prompt the user for confirmation: Use this commit message? [y]es, [e]dit, [n]o: .

If 'y', call m_sys->executeCommand("git", {"commit", "-m", llm_message}).

If 'e', allow the user to enter a new message from the command line, then commit with that.

If 'n', exit without committing.

Input / Output Details
SysInteraction::executeCommand() Method:

Input: A std::string for the command and a std::vector<std::string> for its arguments.

Output: A std::pair containing the command's stdout as a std::string and its exit code as an int.

Core::handleCommit() Method:

Input: A Git repository with changes added to the staging area.

Output (to console): The LLM-generated commit message and confirmation prompts.

Output (to system): An executed git commit command that creates a new commit in the local repository.

3. build & test Command Wrappers
Objective
To implement the build and test commands as wrappers that can execute user-configured commands and use the LLM to analyze any resulting errors.

Component Breakdown & Logic
Updated Component: Core::handleBuild() & Core::handleTest() Methods

Description: These two methods will share nearly identical logic.

Step-by-step Logic:

Use the m_config object to read the build_command (or test_command) string from .camus/config.yml. If the key doesn't exist, print an error.

Parse this command string to separate the executable from its arguments (e.g., 'cmake --build ./build' becomes command cmake and arguments --build, ./build).

Append any additional arguments the user passed on the command line (m_commands.passthrough_args) to this list of arguments.

Call m_sys->executeCommand() with the parsed command and full argument list.

Check the returned exit code. If it's 0, print a success message and return.

If the exit code is non-zero, it signifies a failure. Capture the stderr output from the command.

Construct a prompt for the LLM, e.g., "The following build command failed. Analyze the error and suggest a fix:\n\n[Error Log]:\n" + error_output.

Call m_llm->getCompletion() and display the LLM's analysis to the user.

Input / Output Details
Core::handleBuild() / Core::handleTest() Methods:

Input: A valid .camus/config.yml file and a project state that will either succeed or fail a build/test run.

Output (to console):

The real-time output from the underlying build/test command.

A final success message, or...

A final failure message followed by the LLM's text-based analysis of the error.