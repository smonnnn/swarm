#include "swarm.h"

int main(){
    const char* spirv_path = "./shaders/compiled/add.spv";
    size_t element_count = 256;
    size_t buff_size = element_count * sizeof(float);
    printf("Create context...\n");
    VKCTX ctx = createVkContext();

    printf("Create programs...\n");
    VKPROGRAM programs[2];
    programs[0] = createProgram(ctx, spirv_path);
    programs[1] = createProgram(ctx, spirv_path);

    printf("Create buffers...\n");
    VKBUFFER cpu_buffer = newBuffer(ctx, buff_size, BUF_CPU);
    VKBUFFER bufA       = newBuffer(ctx, buff_size, BUF_GPU);
    VKBUFFER bufB       = newBuffer(ctx, buff_size, BUF_GPU);
    VKBUFFER output     = newBuffer(ctx, buff_size, BUF_GPU);
    
    printf("Map buffer & copy data...\n");
    float* mapped = mapBuffer(ctx, cpu_buffer);
    for(int i = 0; i < element_count; i++){
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
    useBuffers(ctx, programs, buffers, 3);

    VKBUFFER buffersB[3];
    buffersB[0] = output;
    buffersB[1] = output;
    buffersB[2] = output;
    useBuffers(ctx, programs + 1, buffersB, 3);

    //Verify programs...
    verifyVKPROGRAM(programs);
    verifyVKPROGRAM(programs + 1);

    printf("run compute command...\n");
    uint32_t groups = (element_count + 63) / 64;
    runComputeCommand(ctx, programs, 2, groups);

    printf("Copy data back to cpu buffer...\n");
    runCopyCommand(ctx, output, cpu_buffer, 0, 0, output.size);
    
    printf("Map buffer & print results...\n");
    mapped = mapBuffer(ctx, cpu_buffer);
    for(int i = 0; i < element_count; i++){
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
