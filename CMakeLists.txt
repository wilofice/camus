# This file now includes the logic to fetch and build llama.cpp

cmake_minimum_required(VERSION 3.16)
project(camus CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependency Management -----------------------------------------
include(FetchContent)

# 1. CLI11 for command-line parsing
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.4.1
)
FetchContent_MakeAvailable(cli11)

# 2. llama.cpp for LLM interaction
# Using a specific tag for stability and enabling Metal.
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "")
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "")
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "")
set(LLAMA_METAL ON CACHE BOOL "") # Ensure Metal is explicitly enabled

# Add other backend options as needed, e.g., set(LLAMA_CUBLAS ON CACHE BOOL "") for NVIDIA GPU support
FetchContent_Declare(
  llama_cpp
  GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
  GIT_TAG b3219 # A recent, stable release tag
)
FetchContent_MakeAvailable(llama_cpp)

# --- Project Structure ---------------------------------------------

# Automatically find all .cpp files in the src directory.
# This is more robust than listing each file manually.
file(GLOB_RECURSE CAMUS_SOURCES "src/*.cpp")

# --- Executable Target ---------------------------------------------

# Define the main executable for our Camus CLI using the sources found above.
add_executable(
  camus
  ${CAMUS_SOURCES}
)

target_include_directories(camus PUBLIC include)

# Link our executable against the libraries it needs.
# `llama` is the library target created by llama.cpp's build process.
target_link_libraries(camus PRIVATE CLI11::CLI11 llama)

# --- Post-Build Step for Runtime Dependencies ----------------------

# This ensures that the ggml-metal target (which creates default.metallib)
# is built before our main camus target.
add_dependencies(camus ggml-metal)

# After the 'camus' executable is built, copy the required Metal library
# to the same directory, so it can be found at runtime.
# This version uses the explicit path to the generated file.
add_custom_command(
    TARGET camus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_BINARY_DIR}/bin/default.metallib"
            "$<TARGET_FILE_DIR:camus>/"
    COMMENT "Copying Metal library for runtime..."
    DEPENDS ggml-metal # This command depends on the ggml-metal target completing.
)
