#!/bin/bash

# Install dependencies with Homebrew
brew install nlohmann-json yaml-cpp

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Run the generator
./api_doc_generator

# Copy generated files to parent directory
cp openapi.* ..

# Return to parent directory
cd ..

# Remove the build directory
rm -rf build
