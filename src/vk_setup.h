#ifndef VK_SETUP_H
#define VK_SETUP_H

#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_CHECK(x)                                                    \
    do {                                                               \
        VkResult err = x;                                              \
        if (err != VK_SUCCESS) {                                       \
            fprintf(stderr, "Vulkan error: %d\n", err);                \
            exit(1);                                                   \
        }                                                              \
    } while (0)

static inline void *xmalloc(size_t sz)
{
    void *p = malloc(sz);
    if (!p) { fprintf(stderr, "OOM @ %s:%d\n", __FILE__, __LINE__); exit(1); }
    return p;
}

static inline void *xrealloc(void *p, size_t sz)
{
    void *tmp = realloc(p, sz);
    if (!tmp) { fprintf(stderr, "OOM @ %s:%d\n", __FILE__, __LINE__); exit(1); }
    return tmp;
}

#define XMALLOC(sz) xmalloc(sz)
#define XREALLOC(pp, sz) ((pp) = xrealloc(pp, sz))

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    uint32_t queue_family_idx;
    VkDevice device;
    VkQueue queue;
    VkDescriptorPool descriptor_pool;
    VkCommandPool command_pool;
} VKCTX;

VKCTX createVkContext();
void destroyVkContext(VKCTX s);
#endif