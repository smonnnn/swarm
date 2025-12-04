#ifndef VK_BUFFER_H
#define VK_BUFFER_H

#include "vk_setup.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct VKBUFFER {
    VkBuffer       buffer;
    VkDeviceMemory memory;
    VkDeviceSize   size;
} VKBUFFER;

typedef enum {
    BUF_CPU = 0,
    BUF_GPU = 1
} BufferLocation;

VKBUFFER newBuffer(VKCTX ctx, VkDeviceSize size, BufferLocation where);
void destroyBuffer(VKCTX ctx, VKBUFFER buf);

static inline void* mapBuffer(VKCTX ctx, VKBUFFER b) {
    void* p; vkMapMemory(ctx.device, b.memory, 0, b.size, 0, &p); return p;
}

static inline void unmapBuffer(VKCTX ctx, VKBUFFER b) { 
    vkUnmapMemory(ctx.device, b.memory); 
}
#endif