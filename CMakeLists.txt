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

# 4. cpp-httplib for HTTP client functionality
FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(cpp_httplib)

# 5. nlohmann/json for JSON parsing
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# 6. yaml-cpp for YAML configuration parsing
FetchContent_Declare(
  yaml_cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG master
)
FetchContent_MakeAvailable(yaml_cpp)


# --- Project Structure ---------------------------------------------
file(GLOB_RECURSE CAMUS_SOURCES "src/*.cpp")

# Create a library for shared code between main executable and tests
file(GLOB_RECURSE CAMUS_LIB_SOURCES "src/Camus/*.cpp")
add_library(camus_core ${CAMUS_LIB_SOURCES})

# --- Executable Target ---------------------------------------------
add_executable(
  camus
  src/main.cpp
)

# --- Include Directories -------------------------------------------
target_include_directories(camus PUBLIC include)
target_include_directories(camus_core PUBLIC include)
target_include_directories(camus PRIVATE ${cli11_SOURCE_DIR}/include)
target_include_directories(camus_core PRIVATE ${cli11_SOURCE_DIR}/include)

# The dtl library's headers are in its 'dtl' subdirectory.
target_include_directories(camus PRIVATE ${diff_lib_SOURCE_DIR})
target_include_directories(camus_core PRIVATE ${diff_lib_SOURCE_DIR})

# cpp-httplib headers
target_include_directories(camus PRIVATE ${cpp_httplib_SOURCE_DIR})
target_include_directories(camus_core PRIVATE ${cpp_httplib_SOURCE_DIR})

# nlohmann/json headers
target_include_directories(camus PRIVATE ${nlohmann_json_SOURCE_DIR}/single_include)
target_include_directories(camus_core PRIVATE ${nlohmann_json_SOURCE_DIR}/single_include)

# yaml-cpp headers
target_include_directories(camus_core PRIVATE ${yaml_cpp_SOURCE_DIR}/include)

# --- Linker Settings -----------------------------------------------
target_link_libraries(camus_core PRIVATE CLI11::CLI11 llama yaml-cpp)
target_link_libraries(camus PRIVATE camus_core)

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

# --- Tests ---------------------------------------------------------
# Enable testing
enable_testing()

# Add tests subdirectory
add_subdirectory(tests)
