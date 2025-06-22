Camus CLI - Issue & Resolution Log
This document chronicles the technical challenges, errors, and their corresponding solutions encountered during the development of the Camus CLI. It serves as a historical record for documentation and future retrospectives.

Issue #1: Initial llama.cpp API Mismatch
Symptom: The first compilation attempt of LlmInteraction.cpp failed with numerous errors, including no matching function for call to 'llama_backend_init' and many warnings about "deprecated" functions.

Root Cause: The CMakeLists.txt was configured to fetch the latest master branch of llama.cpp, but the C++ code was written against an older, different version of its API. This created a significant function signature mismatch as the library had evolved.

Solution: The problem was resolved by synchronizing the code with a specific, stable version of the library. We modified CMakeLists.txt to fetch a specific GIT_TAG and updated LlmInteraction.cpp to use the corresponding API for that version, ensuring consistency.

Issue #2: C++ unique_ptr with Forward-Declared Types
Symptom: Compilation failed with the error: invalid application of 'sizeof' to an incomplete type 'Camus::LlmInteraction'.

Root Cause: This was a C++ architecture issue. Core.hpp used forward declarations for LlmInteraction and SysInteraction while also containing std::unique_ptr members for them. std::unique_ptr's default deleter needs the full definition of a type to know how to destroy it, but this definition was not visible in the header where the Core class's destructor was being implicitly defined.

Solution: We followed the standard C++ pattern for this scenario (the "PIMPL idiom"). The destructor for the Core class was explicitly declared in Core.hpp (~Core();) and then defined in Core.cpp (Core::~Core() = default;). This forces the compiler to generate the destructor only after it has seen the full class definitions from the .cpp file's includes.

Issue #3: Linker Error for New Source Files
Symptom: After adding the ConfigParser class, the project compiled but failed at the final linking stage with an Undefined symbol: Camus::ConfigParser::ConfigParser(...) error.

Root Cause: The new ConfigParser.cpp file had not been added to the list of source files in CMakeLists.txt, so it was never compiled into an object file for the linker to use.

Solution: The CMakeLists.txt was made more robust. Instead of listing each source file manually, it was updated to use file(GLOB_RECURSE CAMUS_SOURCES "src/*.cpp") to automatically find and include all source files in the src directory, preventing this class of error in the future.

Issue #4: Dependency FetchContent Errors (Multiple Occurrences)
Symptom: The CMake configuration step failed with errors like fatal: repository '...' not found or fatal: invalid reference: '...'.

Root Cause: This issue occurred multiple times with different libraries (llama.cpp and two diff libraries). The root cause was always an incorrect value for either the GIT_REPOSITORY URL or the GIT_TAG in the FetchContent_Declare command.

Solution: In each case, the solution was to carefully verify the correct repository URL and a valid, existing release tag or commit hash on the library's official GitHub page and update the CMakeLists.txt file accordingly. This reinforced the importance of using specific, stable versions for dependencies.

Issue #5: macOS Metal Runtime Error
Symptom: After a successful build, running the camus modify command on macOS failed at runtime with an error indicating that ggml-metal.metal could not find its required header, ggml-common.h.

Root Cause: llama.cpp's Metal backend needs to load a pre-compiled shader library (default.metallib) at runtime. When our camus executable ran, it couldn't find this file in its working directory. As a fallback, llama.cpp tried to compile the shader from source at runtime, which failed because it wasn't in the correct include environment.

Solution: A post-build command was added to CMakeLists.txt. This command ensures that after camus is compiled, CMake copies the default.metallib file from the llama.cpp build directory directly into the same directory as the camus executable. This makes the required runtime file available in a location the application can always find.

Issue #6: Repetitive/Infinite Model Generation
Symptom: The model would output the same phrase or code block repeatedly, never stopping until manually interrupted (Ctrl+C).

Root Cause: The initial text generation loop used a simple llama_sample_token_greedy sampler. This method always picks the single most probable next token. If the model entered a state where the most likely token was, for example, a newline, it would get stuck picking it over and over.

Solution: The sampling logic in LlmInteraction.cpp was completely replaced with a more robust pipeline. This new pipeline uses a combination of sampling methods, including llama_sample_repetition_penalties, top_k, top_p, and temp, to introduce a degree of randomness and explicitly penalize repetition, breaking the loops and leading to more coherent output.

Issue #7: Incorrect or "Chatty" Model Output
Symptom: The model would often include conversational text ("Here is the code you requested:") or markdown fences (```) in its response, despite being instructed not to.

Root Cause: This was traced to two issues: (1) an improper prompt format for the specific instruct-tuned models, and (2) the model's inherent training as a conversational assistant.

Solution: This was fixed with a two-pronged approach:

Prompt Engineering: The prompt construction in Core.cpp was updated to use the exact chat template required by the specific model (e.g., Llama 3 or DeepSeek Coder), using their special control tokens.

Output Cleaning: A clean_llm_output function was implemented in LlmInteraction.cpp to programmatically strip any remaining markdown fences or conversational boilerplate from the model's final output before it is used.

Issue #8: Garbled Output and Degraded Quality Warning
Symptom: Despite all code fixes, the model output remained nonsensical and garbled. The startup log consistently showed a critical warning from llama.cpp: GENERATION QUALITY WILL BE DEGRADED!.

Root Cause: This warning indicated that the issue was not with our code, but with the model file (.gguf) itself. The file had been improperly converted, resulting in a missing or corrupt tokenizer configuration. This meant the model could not correctly understand the text of the prompt.

Solution: The solution was external to the codebase. It required downloading a new, correctly converted GGUF model file from a reliable source on Hugging Face and updating the default_model path in the .camus/config.yml file. This resolved the final quality issue.