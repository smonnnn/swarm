#include "swarm.h"

int main(){
    const char* spirv_path = "./shaders/compiled/add.spv";
    VKCTX ctx = createVkContext();
    VKPROGRAM program = createProgram(ctx, spirv_path);
    VKBUFFER cpu_buffer = newBuffer(ctx, 256, BUF_CPU);
    VKBUFFER bufA = newBuffer(ctx, 256, BUF_GPU);
    VKBUFFER bufB = newBuffer(ctx, 256, BUF_GPU);
    VKBUFFER output = newBuffer(ctx, 256, BUF_GPU);

    float* mapped = mapBuffer(ctx, cpu_buffer);
    for(int i = 0; i < 256; i++){
        mapped[i] = (float) i;
    }
    unmapBuffer(ctx, cpu_buffer);
    
    runCopyCommand(ctx, cpu_buffer, bufA, 0, 0, cpu_buffer.size);
    runCopyCommand(ctx, cpu_buffer, bufB, 0, 0, cpu_buffer.size);
    VKBUFFER buffers[3];
    buffers[0] = bufA;
    buffers[1] = bufB;
    buffers[2] = output;
    useBuffers(ctx, &program, buffers, 3);
    runComputeCommand(ctx, &program, 1);
    runCopyCommand(ctx, output, cpu_buffer, 0, 0, output.size);
    mapped = mapBuffer(ctx, cpu_buffer);
    for(int i = 0; i < output.size; i++){
        printf("%d ", (int) mapped[i]);
    }
    unmapBuffer(ctx, cpu_buffer);
    destroyBuffer(ctx, bufA);
    destroyBuffer(ctx, bufB);
    destroyBuffer(ctx, output);
    destroyBuffer(ctx, cpu_buffer);
    destroyProgram(ctx, &program);
    destroyVkContext(ctx);
}
