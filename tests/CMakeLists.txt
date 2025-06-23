# =================================================================
# tests/CMakeLists.txt
# =================================================================
# CMake configuration for Camus unit tests.

cmake_minimum_required(VERSION 3.10)

# Test executables
set(TEST_EXECUTABLES
    ProjectScannerTest
    ContextBuilderTest
    ResponseParserTest
    SafetyCheckerTest
    TaskClassifierTest
    ModelAbstractionTest
    ModelRegistryTest
    ModelSelectorTest
    LoadBalancerTest
    IntegrationTest
    TestRunner
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external/dtl)

# Common libraries for all tests
set(COMMON_LIBS
    camus_core
    yaml-cpp
)

# ProjectScanner tests
add_executable(ProjectScannerTest ProjectScannerTest.cpp)
target_link_libraries(ProjectScannerTest ${COMMON_LIBS})
target_compile_features(ProjectScannerTest PRIVATE cxx_std_17)

# ContextBuilder tests  
add_executable(ContextBuilderTest ContextBuilderTest.cpp)
target_link_libraries(ContextBuilderTest ${COMMON_LIBS})
target_compile_features(ContextBuilderTest PRIVATE cxx_std_17)

# ResponseParser tests
add_executable(ResponseParserTest ResponseParserTest.cpp)
target_link_libraries(ResponseParserTest ${COMMON_LIBS})
target_compile_features(ResponseParserTest PRIVATE cxx_std_17)

# SafetyChecker tests
add_executable(SafetyCheckerTest SafetyCheckerTest.cpp)
target_link_libraries(SafetyCheckerTest ${COMMON_LIBS})
target_compile_features(SafetyCheckerTest PRIVATE cxx_std_17)

# TaskClassifier tests
add_executable(TaskClassifierTest TaskClassifierTest.cpp)
target_link_libraries(TaskClassifierTest ${COMMON_LIBS})
target_compile_features(TaskClassifierTest PRIVATE cxx_std_17)

# ModelAbstraction tests
add_executable(ModelAbstractionTest ModelAbstractionTest.cpp)
target_link_libraries(ModelAbstractionTest ${COMMON_LIBS})
target_compile_features(ModelAbstractionTest PRIVATE cxx_std_17)

# ModelRegistry tests
add_executable(ModelRegistryTest ModelRegistryTest.cpp)
target_link_libraries(ModelRegistryTest ${COMMON_LIBS})
target_compile_features(ModelRegistryTest PRIVATE cxx_std_17)

# ModelSelector tests
add_executable(ModelSelectorTest ModelSelectorTest.cpp)
target_link_libraries(ModelSelectorTest ${COMMON_LIBS})
target_compile_features(ModelSelectorTest PRIVATE cxx_std_17)

# LoadBalancer tests
add_executable(LoadBalancerTest LoadBalancerTest.cpp)
target_link_libraries(LoadBalancerTest ${COMMON_LIBS})
target_compile_features(LoadBalancerTest PRIVATE cxx_std_17)

# Integration tests
add_executable(IntegrationTest IntegrationTest.cpp)
target_link_libraries(IntegrationTest ${COMMON_LIBS})
target_compile_features(IntegrationTest PRIVATE cxx_std_17)

# Test runner
add_executable(TestRunner TestRunner.cpp)
target_compile_features(TestRunner PRIVATE cxx_std_17)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/TestRunner
    DEPENDS ${TEST_EXECUTABLES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all unit tests"
)

# Individual test targets
add_custom_target(test_project_scanner
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ProjectScannerTest
    DEPENDS ProjectScannerTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ProjectScanner tests"
)

add_custom_target(test_context_builder
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ContextBuilderTest
    DEPENDS ContextBuilderTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ContextBuilder tests"
)

add_custom_target(test_response_parser
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ResponseParserTest
    DEPENDS ResponseParserTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ResponseParser tests"
)

add_custom_target(test_safety_checker
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/SafetyCheckerTest
    DEPENDS SafetyCheckerTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running SafetyChecker tests"
)

add_custom_target(test_task_classifier
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/TaskClassifierTest
    DEPENDS TaskClassifierTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running TaskClassifier tests"
)

add_custom_target(test_model_abstraction
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ModelAbstractionTest
    DEPENDS ModelAbstractionTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ModelAbstraction tests"
)

add_custom_target(test_model_registry
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ModelRegistryTest
    DEPENDS ModelRegistryTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ModelRegistry tests"
)

add_custom_target(test_model_selector
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/ModelSelectorTest
    DEPENDS ModelSelectorTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ModelSelector tests"
)

add_custom_target(test_load_balancer
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/LoadBalancerTest
    DEPENDS LoadBalancerTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running LoadBalancer tests"
)

add_custom_target(test_integration
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/IntegrationTest
    DEPENDS IntegrationTest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Integration tests"
)

# Enable CTest integration
enable_testing()

add_test(NAME ProjectScannerTest COMMAND ProjectScannerTest)
add_test(NAME ContextBuilderTest COMMAND ContextBuilderTest)
add_test(NAME ResponseParserTest COMMAND ResponseParserTest)
add_test(NAME SafetyCheckerTest COMMAND SafetyCheckerTest)
add_test(NAME TaskClassifierTest COMMAND TaskClassifierTest)
add_test(NAME ModelAbstractionTest COMMAND ModelAbstractionTest)
add_test(NAME ModelRegistryTest COMMAND ModelRegistryTest)
add_test(NAME ModelSelectorTest COMMAND ModelSelectorTest)
add_test(NAME LoadBalancerTest COMMAND LoadBalancerTest)
add_test(NAME IntegrationTest COMMAND IntegrationTest)

# Set test properties
set_tests_properties(
    ProjectScannerTest 
    ContextBuilderTest 
    ResponseParserTest 
    SafetyCheckerTest
    TaskClassifierTest
    ModelAbstractionTest
    ModelRegistryTest
    ModelSelectorTest
    LoadBalancerTest
    IntegrationTest
    PROPERTIES 
    TIMEOUT 300  # 5 minute timeout
)

# Print test information
message(STATUS "Configured unit tests:")
foreach(test_exe ${TEST_EXECUTABLES})
    message(STATUS "  - ${test_exe}")
endforeach()