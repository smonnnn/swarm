#!/bin/bash
set -e

echo "Building Vulkan compute library (debug)..."

mkdir -p build

# ---- common flags -----------------------------------------------------------
CFLAGS="-g -O0 -Wall -fPIC"          # -g  → debug info
INC="-Isrc -I/usr/include/vulkan"

# ---- compile ----------------------------------------------------------------
for f in vk_setup vk_buffer vk_command vk_program; do
    echo "Compiling $f.c (debug)..."
    gcc -c $CFLAGS $INC -o build/$f.o src/$f.c
done

echo "Compiling spirv_reflect.c (debug)..."
gcc -c $CFLAGS $INC -o build/spirv_reflect.o src/include/spirv_reflect.c

# ---- static library ---------------------------------------------------------
echo "Creating static library..."
ar rcs build/libswarm.a build/*.o

echo "Done."
echo "Debug library: build/libswarm.a"