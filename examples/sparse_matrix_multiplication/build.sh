export EXAMPLE_NAME="sparse_matrix_multiplication"
mkdir build
mkdir ./build/${EXAMPLE_NAME}
glslangValidator -V "./examples/${EXAMPLE_NAME}/sparse_matrix_multiply.comp" -o "./build/${EXAMPLE_NAME}/sparse_matrix_multiply.spirv"
gcc ./examples/${EXAMPLE_NAME}/main.c -o ./build/${EXAMPLE_NAME}/sparse_matrix_multiplication -I. -Lbuild -lswarm -lvulkan -lm
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LOADER_DEBUG=warn
cd ./build/${EXAMPLE_NAME}/ && ./sparse_matrix_multiplication