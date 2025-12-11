gcc -g -O0 -I src -L build -o ./build/example_add ./examples/add.c -Lbuild -lswarm -lvulkan -lm
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LOADER_DEBUG=warn
./build/example_add 2> validation.log