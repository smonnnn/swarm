#ifndef SWARM_H
#define SWARM_H

#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>

//Enums
typedef enum {
    READ_ONLY,
    WRITE_ONLY,
    READ_AND_WRITE
} BindingLimitations;

typedef enum {
    BUF_CPU = 0,
    BUF_GPU = 1
} BufferLocation;

//Structs
typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    uint32_t queue_family_idx;
    VkDevice device;
    VkQueue queue;
    VkDescriptorPool descriptor_pool;
    VkCommandPool command_pool;
} VKCTX;

typedef struct VKBUFFER {
    VkBuffer       buffer;
    VkDeviceMemory memory;
    VkDeviceSize   size;
} VKBUFFER;

typedef struct{
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorSet descriptor_set;
    VkDescriptorType* buffer_types;
    uint32_t* buffer_indices;
    VkBuffer* buffers;
    size_t buffer_count;
    BindingLimitations* binding_read_write_limitations;
} VKPROGRAM;

//vk_setup
VKCTX createVkContext();
void destroyVkContext(VKCTX s);

//vk_buffer
VKBUFFER newBuffer(VKCTX ctx, VkDeviceSize size, BufferLocation where);
void destroyBuffer(VKCTX ctx, VKBUFFER buf);
static inline void* mapBuffer(VKCTX ctx, VKBUFFER b) {
    void* p; vkMapMemory(ctx.device, b.memory, 0, b.size, 0, &p); return p;
}
static inline void unmapBuffer(VKCTX ctx, VKBUFFER b) { 
    vkUnmapMemory(ctx.device, b.memory); 
}

//vk_program
VKPROGRAM createProgram(VKCTX ctx, const char* shader_path);
void destroyProgram(VKCTX ctx, VKPROGRAM* program);
void useBuffers(VKCTX ctx, VKPROGRAM* program, VKBUFFER* buffers, uint32_t buffer_count);

//vk_command
void runCopyCommand(VKCTX ctx, VKBUFFER from, VKBUFFER to, uint32_t from_offset, uint32_t to_offset, uint32_t size);
void runComputeCommand(VKCTX ctx, VKPROGRAM* programs, uint32_t program_count);
#endif