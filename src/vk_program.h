#ifndef VK_PROGRAM_H
#define VK_PROGRAM_H

#include "vk_setup.h"
#include "vk_buffer.h"

typedef enum {
    READ_ONLY,
    WRITE_ONLY,
    READ_AND_WRITE
} BindingLimitations;

typedef struct{
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkDescriptorSet descriptor_set;
    VkDescriptorType* buffer_types;
    uint32_t* buffer_indices;
    VKBUFFER* buffers;
    size_t buffer_count;
    BindingLimitations* binding_read_write_limitations;
} VKPROGRAM;

typedef struct{
    char* entrypoint;
    BindingLimitations* binding_read_write_limitations;
    VkDescriptorType* buffer_types;
    uint32_t* buffer_indices;
    size_t buffer_count;
    uint32_t* spirv_bytecode;
    size_t spirv_bytecode_length;
} ShaderInfo;

VKPROGRAM createProgram(VKCTX ctx, const char* shader_path);
void destroyProgram(VKCTX ctx, VKPROGRAM* program);
void useBuffers(VKCTX ctx, VKPROGRAM* program, VKBUFFER* buffers, uint32_t buffer_count);
#endif