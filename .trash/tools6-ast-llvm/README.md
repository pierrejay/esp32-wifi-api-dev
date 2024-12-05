# WiFi Manager API Documentation Generator

This tool generates OpenAPI documentation from the WiFi Manager source code by analyzing the AST (Abstract Syntax Tree).

## Prerequisites

- CMake 3.10 or higher
- LLVM/Clang development libraries
- nlohmann_json
- yaml-cpp

## Building

```bash
mkdir build
cd build
cmake ..
make