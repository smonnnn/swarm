#ifndef VK_COMMAND_H
#define VK_COMMAND_H

#include "vk_setup.h"
#include "vk_buffer.h"
#include "vk_program.h"
void runCopyCommand(VKCTX ctx, VKBUFFER from, VKBUFFER to, uint32_t from_offset, uint32_t to_offset, uint32_t size);
void runComputeCommand(VKCTX ctx, VKPROGRAM* programs, uint32_t program_count);

#endif