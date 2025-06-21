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
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "")
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "")
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "")
set(LLAMA_METAL ON CACHE BOOL "") 
FetchContent_Declare(
  llama_cpp
  GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
  GIT_TAG b3219 
)
FetchContent_MakeAvailable(llama_cpp)

# 3. dtl (Diff Template Library) for generating diffs
# Corrected GIT_TAG to a valid release version.
FetchContent_Declare(
  diff_lib
  GIT_REPOSITORY https://github.com/cubicdaiya/dtl.git
  GIT_TAG v1.21
)
FetchContent_MakeAvailable(diff_lib)


# --- Project Structure ---------------------------------------------
file(GLOB_RECURSE CAMUS_SOURCES "src/*.cpp")

# --- Executable Target ---------------------------------------------
add_executable(
  camus
  ${CAMUS_SOURCES}
)

# --- Include Directories -------------------------------------------
target_include_directories(camus PUBLIC include)

# The dtl library's headers are in its 'dtl' subdirectory.
target_include_directories(camus PRIVATE ${diff_lib_SOURCE_DIR})

# --- Linker Settings -----------------------------------------------
target_link_libraries(camus PRIVATE CLI11::CLI11 llama)

# --- Post-Build Step for Runtime Dependencies ----------------------
add_dependencies(camus ggml-metal)

add_custom_command(
    TARGET camus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_BINARY_DIR}/bin/default.metallib"
            "$<TARGET_FILE_DIR:camus>/"
    COMMENT "Copying Metal library for runtime..."
    DEPENDS ggml-metal # This command depends on the ggml-metal target completing.
)
