#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Run the generator
./gen

# Return to parent directory
cd ..

# Clean up build directory
rm -rf build 