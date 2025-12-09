gcc ./examples/sparse_matrix_multiply.c -o ./build/sparse_matrix_multiply -I. -Lbuild -lswarm -lvulkan -lm
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LOADER_DEBUG=warn
./build/sparse_matrix_multiply