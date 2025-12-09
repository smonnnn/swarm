#include "vk_setup.h"

VkInstance createInstance(const char** extensions, uint32_t extensionCount) {
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "SWARM",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "SWARM",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2,
    };

    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
    };

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&info, NULL, &instance));
    return instance;
}

VkPhysicalDevice userPickDevice(VkInstance instance) {
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, NULL));
    if (count == 0) {
        fprintf(stderr, "No Vulkan-capable GPU found!\n");
        exit(1);
    }

    VkPhysicalDevice *devices = XMALLOC(sizeof(VkPhysicalDevice) * count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, devices));

    printf("Available Vulkan devices:\n");
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        printf(" [%u] %s\n", i, props.deviceName);
    }

    printf("Select device index: ");
    uint32_t idx;
    scanf("%u", &idx);

    if (idx >= count) {
        fprintf(stderr, "Invalid device index.\n");
        exit(1);
    }
    VkPhysicalDevice chosen = devices[idx];
    free(devices);
    return chosen;
}

int32_t getQueueFamily(VkPhysicalDevice device, int32_t flags) {
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &qCount, NULL);

    VkQueueFamilyProperties *props = XMALLOC(sizeof(*props) * qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &qCount, props);

    int32_t result = -1;
    for (uint32_t i = 0; i < qCount; i++) {
        if (props[i].queueFlags & flags) {
            result = i;
            break;
        }
    }

    free(props);
    if (result < 0) {
        fprintf(stderr, "This device does not support the given queue family flags!\n");
        exit(1);
    }
    return result;
}

VkDevice createLogicalDevice(VkPhysicalDevice phys, uint32_t computeFamily, void* create_info_pnext, const char** extensions, uint32_t extensionCount) {
    float priority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = computeFamily,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .pNext = create_info_pnext,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
    };

    VkDevice device;
    VK_CHECK(vkCreateDevice(phys, &deviceInfo, NULL, &device));
    return device;
}

VkQueue getQueue(VkDevice device, int32_t queue_family_index){
    VkQueue q;
    vkGetDeviceQueue(device, queue_family_index, 0, &q);
    return q;
}

VkDescriptorPool createDescriptorPool(VkDevice device){
    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 24
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 32,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };

    VkDescriptorPool pool;
    vkCreateDescriptorPool(device, &poolInfo, NULL, &pool);
    return pool;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t queueIndex){
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueIndex,
    };

    VkCommandPool cmdPool;
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, NULL, &cmdPool));
    return cmdPool;
}

VKCTX createVkContext(){
    const char* instanceExts[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };

    const char* deviceExts[] = {
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME, //atomic float
        //VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        //VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        //VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        //VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        //VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        //VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
    };

    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_float_featues = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        .pNext = NULL,
        .shaderBufferFloat32Atomics = VK_TRUE,      // For buffer atomics
        .shaderBufferFloat32AtomicAdd = VK_TRUE,    // For atomicAdd() on buffers
        .shaderSharedFloat32Atomics = VK_TRUE,      // For shared memory atomics
        .shaderSharedFloat32AtomicAdd = VK_TRUE,    // For atomicAdd() in shared memory
    };

    VkPhysicalDeviceVulkan13Features vk13 = {0};
    vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13.pNext = &atomic_float_featues;

    VkPhysicalDeviceVulkan12Features vk12 = {0};
    vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vk12.pNext = &vk13;
    vk12.bufferDeviceAddress = VK_TRUE;
    vk12.descriptorIndexing = VK_TRUE;
    vk12.timelineSemaphore = VK_TRUE;

    VKCTX ctx = {0};
    printf("Creating instance...\n");
    ctx.instance = createInstance(instanceExts, sizeof(instanceExts) / sizeof(char*));
    printf("Picking device...\n");
    ctx.physical_device = userPickDevice(ctx.instance);
    printf("Selecting compute queue family...\n");
    ctx.queue_family_idx = getQueueFamily(ctx.physical_device, VK_QUEUE_COMPUTE_BIT);
    printf("Creating logical device...\n");
    ctx.device = createLogicalDevice(ctx.physical_device, ctx.queue_family_idx, &vk12, deviceExts, sizeof(deviceExts) / sizeof(char*));
    printf("Picking queue...\n");
    ctx.queue = getQueue(ctx.device, ctx.queue_family_idx);
    printf("Creating descriptor pool...\n");
    ctx.descriptor_pool = createDescriptorPool(ctx.device);
    printf("Creating command pool...\n");
    ctx.command_pool = createCommandPool(ctx.device, ctx.queue_family_idx);
    return ctx;
}    

void destroyVkContext(VKCTX s){
    vkDeviceWaitIdle(s.device);
    vkDestroyDescriptorPool(s.device, s.descriptor_pool, NULL);
    vkDestroyCommandPool(s.device, s.command_pool, NULL);
    vkDestroyDevice(s.device, NULL);
    vkDestroyInstance(s.instance, NULL);
}
