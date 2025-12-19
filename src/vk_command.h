#ifndef VK_COMMAND_H
#define VK_COMMAND_H

#include "vk_setup.h"
#include "vk_buffer.h"
#include "vk_program.h"
VkCommandBuffer startCommand(VKCTX ctx);
void copyBuffer(VKCTX ctx, VkCommandBuffer cmd, VKBUFFER from, VKBUFFER to, uint32_t from_offset, uint32_t to_offset, uint32_t size);
void runProgram(VKCTX ctx, VkCommandBuffer cmd, VKPROGRAM program, VKBUFFER indirect);
void submitCommand(VKCTX ctx, VkCommandBuffer cmd);
void copyBufferSlow(VKCTX ctx, VKBUFFER from, VKBUFFER to, uint32_t from_offset, uint32_t to_offset, uint32_t size);
#endif