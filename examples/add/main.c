#include "../../swarm.h"

/* How I would like to define kernels in the future somehow:
#PRAGMA SHADER_BEGIN(local_size=64)
void kernel_add(typedefstruct_or_valuetype* outputs, typedefstruct_or_valuetype* buffername1, typedefstruct_or_valuetype* buffername2...){
    uint32_t x, uint32_t y, uint32_t z; always defined.
    ///Normal GLSL code pretty much, with simpler names etc.:
    if (x >= outputs.length() || x >= buffername1.length() || x >= buffername2.length()) return;
    outputs[x] = buffername1[x] + buffername2[x];
}
#PRAGMA SHADER_END

How I would have a GPU function be called from C code:
...
VKBUFFER gpu_data1 = to_gpu(data1*, datalen1);
VKBUFFER gpu_data2 = to_gpu(data2*, datalen2);

kernel_add(gpu_data1, gpu_data2);
...

funtion always returns void and doesn't wait on the gpu operation to be complete, memory barriers are automatically placed for each buffer for the program.
to get the data and use it on the cpu, a function that waits on the memory barries of the buffer and copies data to the CPU host is used like so:
from_gpu(gpu_data1, data1); //lenght is stored in the VKBUFFER struct.
*/

int main(){
    const char* spirv_path = "./add.spirv";
    size_t element_count = 256;
    size_t buff_size = element_count * sizeof(float);
    printf("Create context...\n");
    VKCTX ctx = createVkContext();

    printf("Create programs...\n");
    VKPROGRAM programs[2] = {0};
    programs[0] = createProgram(ctx, spirv_path);
    printf("A\n");
    printf("%s\n", spirv_path);
    programs[1] = createProgram(ctx, spirv_path);
    printf("A\n");

    printf("Create buffers...\n");
    VKBUFFER cpu_buffer = newBuffer(ctx, buff_size, BUF_CPU);
    VKBUFFER bufA       = newBuffer(ctx, buff_size, BUF_GPU);
    VKBUFFER bufB       = newBuffer(ctx, buff_size, BUF_GPU);
    VKBUFFER output     = newBuffer(ctx, buff_size, BUF_GPU);
    VKBUFFER indirect   = newBuffer(ctx, 3 * sizeof(uint32_t), BUF_INDIRECT);
    
    printf("Map buffer & copy data...\n");
    float* mapped = mapBuffer(ctx, cpu_buffer);
    for(int i = 0; i < element_count; i++){
        mapped[i] = (float) i;
    }
    unmapBuffer(ctx, cpu_buffer);

    printf("Copy to GPU buffers...\n");
    copyBufferSlow(ctx, cpu_buffer, bufA, 0, 0, cpu_buffer.size);
    copyBufferSlow(ctx, cpu_buffer, bufB, 0, 0, cpu_buffer.size);
    
    //map buffer again and copy indirect value to gpu
    uint32_t* m = mapBuffer(ctx, cpu_buffer);
    uint32_t groups = (element_count + 63) / 64;
    m[0] = groups;
    m[1] = 1;
    m[2] = 1;
    unmapBuffer(ctx, cpu_buffer);
    copyBufferSlow(ctx, cpu_buffer, indirect, 0, 0, indirect.size);

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

    printf("run compute command...\n");
    VkCommandBuffer cmd = startCommand(ctx);
    runProgram(ctx, cmd, programs[0], indirect);
    runProgram(ctx, cmd, programs[1], indirect);
    submitCommand(ctx, cmd);

    printf("Copy data back to cpu buffer...\n");
    copyBufferSlow(ctx, output, cpu_buffer, 0, 0, output.size);
    
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
    destroyBuffer(ctx, indirect);
    destroyProgram(ctx, spirv_path);
    destroyVkContext(ctx);
    printf("Fin.\n");
}
