#include "helper.h"
#define STAGING_BUFFER_SIZE (16 * 1024 * 1024)   //16MB buffer for copying data onto the GPU.
#define LARGE_THRESHOLD    (128 * 1024 * 1024)   //if large enough, create dedicated staging buffer.

static VKCTX ctx = {0};
static int ctx_initialized = 0;
VKCTX* get_ctx(){
    if (!ctx_initialized) {
        ctx = createVkContext();
        ctx_initialized = 1;
    }
    return &ctx;
}

static VKBUFFER staging = {0};
static void* mapped_staging;
static int staging_initialized = 0;
VKBUFFER* get_staging_buffer(){
    VKCTX* ctx = get_ctx();
    if (!staging_initialized) {
        staging = newBuffer(*ctx, STAGING_BUFFER_SIZE, BUF_CPU);
        mapped_staging = mapBuffer(*ctx, staging);
        staging_initialized = 1;
    }
    return &staging;
}

void cleanup(){
    if(!ctx_initialized) return;
    VKCTX* ctx = get_ctx();
    if(staging_initialized){
        unmapBuffer(*ctx, staging);
        destroyBuffer(*ctx, staging);
    }
    destroyVkContext(*ctx);
}

//Maybe clear up the code a bit with regards to mapping and the two different buffers. Maybe do buffer loading while copying data on the CPU, or switch to memory mapped buffers.
VKBUFFER toGPU(void* data, size_t size){
    VKCTX* ctx = get_ctx();
    VKBUFFER buffer = newBuffer(*ctx, size, BUF_GPU);
    VKBUFFER staging;
    if(size <= LARGE_THRESHOLD){
        int segment_size;
        staging = *get_staging_buffer();
        for(int i = 0; i < size; i += STAGING_BUFFER_SIZE){
            segment_size = min(STAGING_BUFFER_SIZE, (size - i));
            memcpy(mapped_staging, (char*)data + i, segment_size);
            copyBufferSlow(*ctx, staging, buffer, 0, i, segment_size);
        }
    } else {
        staging = newBuffer(*ctx, size, BUF_CPU);
        void* mapped = mapBuffer(*ctx, staging);
        memcpy(mapped, data, size);
        unmapBuffer(*ctx, staging);
        copyBufferSlow(*ctx, staging, buffer, 0, 0, size);
        destroyBuffer(*ctx, staging);
    }
    return buffer;
}

void fromGPU(VKBUFFER buffer, void* output){
    VKCTX* ctx = get_ctx();
    VKBUFFER staging;
    if(buffer.size <= LARGE_THRESHOLD){
        int segment_size;
        staging = *get_staging_buffer();
        for(int i = 0; i < buffer.size; i += STAGING_BUFFER_SIZE){
            segment_size = min(STAGING_BUFFER_SIZE, (buffer.size - i));
            copyBufferSlow(*ctx, buffer, staging, i, 0, segment_size);
            memcpy((char*)output + i, mapped_staging, segment_size);
        }
    } else {
        staging = newBuffer(*ctx, buffer.size, BUF_CPU);
        void* mapped = mapBuffer(*ctx, staging);
        copyBufferSlow(*ctx, buffer, staging, 0, 0, buffer.size);
        memcpy(output, mapped, buffer.size);
        unmapBuffer(*ctx, staging);
        destroyBuffer(*ctx, staging);
    }
}