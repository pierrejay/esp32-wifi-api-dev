cmake_minimum_required(VERSION 3.10)
project(wifi_manager_api_doc)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set LLVM paths explicitly for Homebrew
set(LLVM_DIR "/usr/local/opt/llvm/lib/cmake/llvm")
set(Clang_DIR "/usr/local/opt/llvm/lib/cmake/clang")

# Add LLVM and Clang to the path
list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/llvm/lib/cmake/llvm")
list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/llvm/lib/cmake/clang")

# Set additional flags for LLVM
set(LDFLAGS "-L/usr/local/opt/llvm/lib")
set(CPPFLAGS "-I/usr/local/opt/llvm/include")

# Find required packages
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Add LLVM definitions and include directories
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})

# Find yaml-cpp with pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)
include_directories(${YAML_CPP_INCLUDE_DIRS})
link_directories(${YAML_CPP_LIBRARY_DIRS})

# Find nlohmann_json
find_package(nlohmann_json REQUIRED)

# Include project directories
include_directories(${CMAKE_SOURCE_DIR}/../src)
include_directories(${CMAKE_SOURCE_DIR}/../lib)

# Add executable
add_executable(api_doc_gen 
    main.cpp
)

# Link libraries
target_link_libraries(api_doc_gen
    PRIVATE
    clang
    clangTooling
    clangBasic
    clangASTMatchers
    nlohmann_json::nlohmann_json
    ${YAML_CPP_LIBRARIES}
)