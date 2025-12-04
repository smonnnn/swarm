#include "vk_buffer.h"

VKBUFFER newBuffer(VKCTX ctx, VkDeviceSize size, BufferLocation where){
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                             | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                             | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkBufferCreateInfo bufInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VKBUFFER buf = {0};
    buf.size = size;
    VK_CHECK(vkCreateBuffer(ctx.device, &bufInfo, NULL, &buf.buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx.device, buf.buffer, &req);

    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(ctx.physical_device, &props);

    VkMemoryPropertyFlags wanted = (where == BUF_CPU)
                                 ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                                 : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t index = 0;
    for (; index < props.memoryTypeCount; ++index)
        if ((req.memoryTypeBits & (1u << index)) &&
            (props.memoryTypes[index].propertyFlags & wanted) == wanted)
            break;

    if (index == props.memoryTypeCount) {
        fprintf(stderr, "no suitable memory type\n");
        exit(1);
    }

    VkMemoryAllocateFlagsInfo flags = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };

    VkMemoryAllocateInfo alloc = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &flags,
        .allocationSize  = req.size,
        .memoryTypeIndex = index,
    };

    VK_CHECK(vkAllocateMemory(ctx.device, &alloc, NULL, &buf.memory));
    VK_CHECK(vkBindBufferMemory(ctx.device, buf.buffer, buf.memory, 0));
    return buf;
}

void destroyBuffer(VKCTX ctx, VKBUFFER buf){
    vkDestroyBuffer(ctx.device, buf.buffer, NULL);
    vkFreeMemory(ctx.device, buf.memory, NULL);
}