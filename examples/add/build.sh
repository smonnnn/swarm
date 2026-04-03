mkdir build
mkdir ./build/add
glslangValidator -V "./examples/add/add.comp" -o "./build/add/add.spirv"
gcc ./examples/add/main.c -o ./build/add/add -I. -Lbuild -lswarm -lvulkan -lm
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LOADER_DEBUG=warn
cd ./build/add && ./add
