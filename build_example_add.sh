gcc ./examples/add.c -o ./build/example_add -I. -Lbuild -lswarm -lvulkan -lm
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LOADER_DEBUG=warn
./build/example_add