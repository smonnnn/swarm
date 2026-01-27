#include "helper.h"
#define STAGING_BUFFER_SIZE (512 * 1024 * 1024)   //32MB buffer for copying data onto the GPU.

VKCTX* get_ctx(){
    static VKCTX ctx = {0};
    static int initialized = 0;
    
    if (!initialized) {
        ctx = createVkContext();
        initialized = 1;
    }
    
    return &ctx;
}

VKBUFFER* get_staging_buffer(){
    VKCTX* ctx = get_ctx();
    static VKBUFFER staging = {0};
    static int initialized = 0;

    if (!initialized) {
        staging = newBuffer(*ctx, STAGING_BUFFER_SIZE, BUF_CPU);
        initialized = 1;
    }
    
    return &staging;
}

VKBUFFER to_gpu(void* data, size_t size){
    VKCTX* ctx = get_ctx();
    VKBUFFER* staging = get_staging_buffer();
    VKBUFFER buffer = newBuffer(*ctx, size, BUF_GPU);
    void* mapped;
    int segment_size;
    for(int i = 0; i < size; i += STAGING_BUFFER_SIZE){
        segment_size = min(STAGING_BUFFER_SIZE, (size - i));
        mapped = mapBuffer(*ctx, *staging);
        memcpy(mapped, data, segment_size);
        unmapBuffer(*ctx, *staging);
        copyBufferSlow(*ctx, mapped, buffer, i, i, segment_size);
    }
    return buffer;
}

