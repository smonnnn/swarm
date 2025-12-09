#!/bin/bash

set -e

echo "Building Vulkan compute library..."

# Create output directory
mkdir -p build

# Compile each source file separately
echo "Compiling vk_setup.c..."
gcc -c -O2 -Wall -fPIC \
    src/vk_setup.c \
    -Isrc \
    -I/usr/include/vulkan \
    -o build/vk_setup.o

echo "Compiling vk_buffer.c..."
gcc -c -O2 -Wall -fPIC \
    src/vk_buffer.c \
    -Isrc \
    -I/usr/include/vulkan \
    -o build/vk_buffer.o

echo "Compiling vk_command.c..."
gcc -c -O2 -Wall -fPIC \
    src/vk_command.c \
    -Isrc \
    -I/usr/include/vulkan \
    -o build/vk_command.o

echo "Compiling vk_program.c..."
gcc -c -O2 -Wall -fPIC \
    src/vk_program.c \
    -Isrc \
    -I/usr/include/vulkan \
    -o build/vk_program.o

echo "Compiling spirv_reflect.c..."
gcc -c -O2 -Wall -fPIC \
    src/include/spirv_reflect.c \
    -Isrc \
    -I/usr/include/vulkan \
    -o build/spirv_reflect.o

# Create static library
echo "Creating static library..."
ar rcs build/libswarm.a build/vk_setup.o build/vk_buffer.o build/vk_command.o build/vk_program.o build/spirv_reflect.o

echo "Build complete!"
echo "Library: build/libswarm.a"
echo "Object files: build/vk_*.o"