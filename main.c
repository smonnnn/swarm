#include "swarm.h"

int main(){
    const char* spirv_path = "./shaders/compiled/add.spv";
    printf("Create context...\n");
    VKCTX ctx = createVkContext();

    printf("Create program...\n");
    VKPROGRAM* program = createProgram(ctx, spirv_path);

    printf("Create buffers...\n");
    VKBUFFER cpu_buffer = newBuffer(ctx, 256 * sizeof(float), BUF_CPU);
    VKBUFFER bufA = newBuffer(ctx, 256 * sizeof(float), BUF_GPU);
    VKBUFFER bufB = newBuffer(ctx, 256 * sizeof(float), BUF_GPU);
    VKBUFFER output = newBuffer(ctx, 256 * sizeof(float), BUF_GPU);
    
    printf("Map buffer & copy data...\n");
    float* mapped = mapBuffer(ctx, cpu_buffer);
    for(int i = 0; i < 256; i++){
        mapped[i] = (float) i;
    }
    unmapBuffer(ctx, cpu_buffer);

    printf("Copy to GPU buffers...\n");
    runCopyCommand(ctx, cpu_buffer, bufA, 0, 0, cpu_buffer.size);
    runCopyCommand(ctx, cpu_buffer, bufB, 0, 0, cpu_buffer.size);

    printf("Bind buffers to program...\n");
    VKBUFFER buffers[3];
    buffers[0] = bufA;
    buffers[1] = bufB;
    buffers[2] = output;
    useBuffers(ctx, program, buffers, 3);
    
    verifyVKPROGRAM(program);

    printf("run compute command...\n");
    uint32_t numElems = (256 * sizeof(float)) /sizeof(float);
    uint32_t groups = (numElems + 63) / 64;
    runComputeCommand(ctx, program, 1, groups);
    
    printf("Copy data back to cpu buffer...\n");
    runCopyCommand(ctx, output, cpu_buffer, 0, 0, output.size);
    
    printf("Map buffer & print results...\n");
    mapped = mapBuffer(ctx, cpu_buffer);
    for(int i = 0; i < 256; i++){
        printf("%d ", (int) mapped[i]);
    }
    printf("\n");
    unmapBuffer(ctx, cpu_buffer);

    printf("Destroy buffers, program and context...\n");
    destroyBuffer(ctx, bufA);
    destroyBuffer(ctx, bufB);
    destroyBuffer(ctx, output);
    destroyBuffer(ctx, cpu_buffer);
    destroyProgram(ctx, spirv_path);
    destroyVkContext(ctx);
    printf("Fin.\n");
}
