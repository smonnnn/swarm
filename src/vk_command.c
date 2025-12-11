#include "vk_command.h"

void runCopyCommand(VKCTX ctx, VKBUFFER from, VKBUFFER to, uint32_t from_offset, uint32_t to_offset, uint32_t size){
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkBufferCopy copyRegion = {
        .srcOffset = from_offset,
        .dstOffset = to_offset,
        .size = size
    };
    vkCmdCopyBuffer(cmd, from.buffer, to.buffer, 1, &copyRegion);

    VK_CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };
    VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    VK_CHECK(vkCreateFence(ctx.device, &fi, NULL, &fence));
    vkQueueSubmit(ctx.queue, 1, &si, fence);
    vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkFreeCommandBuffers(ctx.device, ctx.command_pool, 1, &cmd);
    vkDestroyFence(ctx.device, fence, NULL);
}

void runComputeCommand(VKCTX ctx, VKPROGRAM* programs, uint32_t program_count, VKBUFFER indirect){
    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device, &cbai, &cmd);

    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };

    vkBeginCommandBuffer(cmd, &bi);
    VkBufferMemoryBarrier b = {
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = indirect.buffer,
        .offset              = 0,
        .size                = indirect.size
    };
    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                        0, 0, NULL, 1, &b, 0, NULL);
    for(int i = 0; i < program_count; i++){        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, programs[i].pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, programs[i].pipeline_layout, 0, 1, &programs[i].descriptor_set, 0, NULL);
        
        vkCmdDispatchIndirect(cmd, indirect.buffer, 0);
        //vkCmdDispatch(cmd, 1, 1, 1);
        VkBufferMemoryBarrier barriers[programs[i].buffer_count]; //Should be the max amount of barriers possible for the given buffers.
        uint32_t barrierCount = 0;

        for (uint32_t j = 0; j < programs[i].buffer_count; ++j) {
            BindingLimitations lim = programs[i].binding_read_write_limitations[j];
            if (lim == WRITE_ONLY || lim == READ_AND_WRITE) {
                barriers[barrierCount++] = (VkBufferMemoryBarrier){
                    .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_READ_BIT,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer              = programs[i].buffers[j].buffer,
                    .offset              = 0,
                    .size                = VK_WHOLE_SIZE
                };
            }
        }

        if (barrierCount)
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   /* producer stage */
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |  /* consumer stage */
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 0, 0, NULL, barrierCount, barriers, 0, NULL);
    }

    vkEndCommandBuffer(cmd);
    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };
    VkFenceCreateInfo fi = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    VK_CHECK(vkCreateFence(ctx.device, &fi, NULL, &fence));
    vkQueueSubmit(ctx.queue, 1, &si, fence);
    vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkFreeCommandBuffers(ctx.device, ctx.command_pool, 1, &cmd);
    vkDestroyFence(ctx.device, fence, NULL);
}