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
# Using a specific tag for stability instead of master branch.
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "")
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "")
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "")
# set(LLAMA_METAL OFF CACHE BOOL "") # <--- ADDED THIS LINE TO FIX THE BUILD ON MACOS

# Add other backend options as needed, e.g., set(LLAMA_CUBLAS ON CACHE BOOL "") for NVIDIA GPU support
FetchContent_Declare(
  llama_cpp
  GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
  GIT_TAG b3219 # A recent, stable release tag
)
FetchContent_MakeAvailable(llama_cpp)

# --- Executable Target ---------------------------------------------
add_executable(
  camus
  src/main.cpp
  src/Camus/CliParser.cpp
  src/Camus/Core.cpp
  src/Camus/LlmInteraction.cpp
  src/Camus/SysInteraction.cpp
)

target_include_directories(camus PUBLIC include)

# Link our executable against the libraries it needs.
# `llama` is the library target created by llama.cpp's build process.
target_link_libraries(camus PRIVATE CLI11::CLI11 llama)
