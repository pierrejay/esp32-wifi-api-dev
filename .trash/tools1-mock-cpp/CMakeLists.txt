cmake_minimum_required(VERSION 3.10)
project(api_doc_generator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(nlohmann_json 3.11.2 REQUIRED)

# Set yaml-cpp paths manually
set(YAML_CPP_INCLUDE_DIR "/usr/local/include")
set(YAML_CPP_LIBRARY "/usr/local/lib/libyaml-cpp.dylib")

# Add executable
add_executable(api_doc_generator api_doc_generator.cpp)

# Link libraries
target_link_libraries(api_doc_generator PRIVATE
    nlohmann_json::nlohmann_json
    ${YAML_CPP_LIBRARY}
)

# Include directories
target_include_directories(api_doc_generator PRIVATE
    ${YAML_CPP_INCLUDE_DIR}
)
