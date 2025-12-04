#!/bin/bash

set -e

mkdir -p shaders/compiled

echo "Compiling GLSL shaders to SPIR-V..."

echo "Searching for GLSL compute shaders..."
SHADER_COUNT=0

for shader in shaders/*.comp; do
    if [ -f "$shader" ]; then
        SHADER_COUNT=$((SHADER_COUNT + 1))
        filename=$(basename "$shader" .comp)
        output_file="./shaders/compiled/${filename}.spv"
        
        echo "Compiling: $shader -> $output_file"
        
        glslangValidator -V "$shader" -o "$output_file"
        
        echo "  ✓ Compiled successfully"
    fi
done

if [ $SHADER_COUNT -eq 0 ]; then
    echo "No .comp files found in shaders/ directory"
    exit 1
fi

echo ""
echo "Shader compilation complete!"
echo "Compiled $SHADER_COUNT shader(s) to compiled/ directory"
echo ""
echo "Available shaders:"
ls -la ./shaders/compiled/*.spv 2>/dev/null || echo "No SPIR-V files generated"