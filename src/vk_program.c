#include "vk_program.h"
#include "include/hashmap.h"
#include "include/spirv_reflect.h"

VkDescriptorSetLayout getDescriptorSetLayout(VKCTX ctx, VKPROGRAM* program, ShaderInfo s){
    VkDescriptorSetLayoutBinding bindings[program->buffer_count];
    for (uint32_t i = 0; i < program->buffer_count; ++i) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding         = program->buffer_indices[i],   // slot index
            .descriptorType  = program->buffer_types[i],     // SSBO, UBO, image, …
            .descriptorCount = 1,                     // array size (1 = single)
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        };
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = program->buffer_count,
        .pBindings    = bindings,
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &info, NULL, &layout));
    return layout;
}

VkPipelineLayout getPipelineLayout(VKCTX ctx, VkDescriptorSetLayout setLayout){
    VkPipelineLayoutCreateInfo ci = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &setLayout,
    };
    VkPipelineLayout plo;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &ci, NULL, &plo));
    return plo;
}

static inline uint32_t word(const uint32_t* p, size_t off) { return p[off]; }
static inline uint32_t opcode(const uint32_t* p, size_t off) { return p[off] & 0xFFFFu; }
static inline uint32_t length(const uint32_t* p, size_t off) { return p[off] >> 16; }

ShaderInfo readShader(VKPROGRAM* program, const char* shader_path){
    ShaderInfo s = {0};

    FILE* f = fopen(shader_path, "rb");
    if (!f) {
        printf("Could not open path: %s\n", shader_path);
        exit(0);
    }
    fseek(f, 0, SEEK_END);
    size_t code_size = ftell(f);
    rewind(f);
    uint32_t* code = (uint32_t*)XMALLOC(code_size);
    fread(code, 1, code_size, f);
    fclose(f);

    SpvReflectShaderModule mod;
    SpvReflectResult res = spvReflectCreateShaderModule(code_size, code, &mod);
    if (res != SPV_REFLECT_RESULT_SUCCESS) {
        printf("SPIRV-Reflect failed for %s\n", shader_path);
        exit(EXIT_FAILURE);
    }

    uint32_t count = 0;
    spvReflectEnumerateDescriptorBindings(&mod, &count, NULL);
    SpvReflectDescriptorBinding** binds = XMALLOC(count * sizeof *binds);
    spvReflectEnumerateDescriptorBindings(&mod, &count, binds);;

    program->buffer_count = count;
    if(!(mod.entry_point_name) || strlen(mod.entry_point_name) == 1){
        s.entrypoint = "main";
    } else {
        s.entrypoint = XMALLOC(strlen(mod.entry_point_name) + 1);
        strcpy(s.entrypoint, mod.entry_point_name);
    }

    s.spirv_bytecode = XMALLOC(code_size);
    memcpy(s.spirv_bytecode, code, code_size);
    s.spirv_bytecode_length = code_size;
    for (uint32_t i = 0; i < count; ++i) {
        program->buffer_indices[i] = binds[i]->binding;

        /* map descriptor type */
        switch (binds[i]->descriptor_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            program->buffer_types[i] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            program->buffer_types[i] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        default:   /* images / samplers / etc. */
            program->buffer_types[i] = VK_DESCRIPTOR_TYPE_MAX_ENUM; /* or your fallback */
            break;
        }

        /* map read/write usage */
        bool readOnly  = (binds[i]->decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
        bool writeOnly = (binds[i]->accessed & SPV_REFLECT_DECORATION_NON_READABLE);

        program->binding_read_write_limitations[i] = READ_AND_WRITE;
        if (readOnly || writeOnly){
            program->binding_read_write_limitations[i] = readOnly ? READ_ONLY : WRITE_ONLY;
        }
    }

    spvReflectDestroyShaderModule(&mod);
    return s;
}

VkPipeline createPipeline(VKCTX ctx, VkPipelineLayout pipelineLayout, ShaderInfo shader_info){
    VkShaderModuleCreateInfo smi = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_info.spirv_bytecode_length,
        .pCode = shader_info.spirv_bytecode,
    };

    VkShaderModule shader;
    VK_CHECK(vkCreateShaderModule(ctx.device, &smi, NULL, &shader));    
    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader,
        .pName = shader_info.entrypoint
    };

    VkComputePipelineCreateInfo pci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = pipelineLayout
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &pci, NULL, &pipeline));

    vkDestroyShaderModule(ctx.device, shader, NULL);
    free(shader_info.spirv_bytecode);
    shader_info.spirv_bytecode_length = 0;
    return pipeline;
}

static struct hashmap_s program_map;
static int program_map_initialized = 0;

VKPROGRAM createProgram(VKCTX ctx, const char* shader_path){
    printf("Shader path: %s\n", shader_path);
    if (!program_map_initialized) {
        if (0 != hashmap_create(1, &program_map)) {
            printf("Error creating hashmap for program cache.\n");
            exit(1);
        }
        program_map_initialized = 1;
    }
    VKPROGRAM* cached = hashmap_get(&program_map, shader_path, strlen(shader_path));
    if (cached) return *cached;

    VKPROGRAM* program = XMALLOC(sizeof(VKPROGRAM));
    ShaderInfo shader_info = readShader(program, shader_path);
    program->descriptor_set_layout = getDescriptorSetLayout(ctx, program, shader_info);
    program->pipeline_layout = getPipelineLayout(ctx, program->descriptor_set_layout);
    program->pipeline = createPipeline(ctx, program->pipeline_layout, shader_info);
    hashmap_put(&program_map, shader_path, strlen(shader_path), program);
    return *program;
}

static struct hashmap_s descriptor_map;
static int descriptor_map_initialized = 0;

//Note: Buffers are in order of bindings so their index in the array corresponds to their binding idx.
void useBuffers(VKCTX ctx, VKPROGRAM* program, VKBUFFER* buffers, size_t buffer_count){
    if (!descriptor_map_initialized) {
        if (0 != hashmap_create(1, &descriptor_map)) {
            printf("Error creating hashmap for program cache.\n");
            exit(1);
        }
        descriptor_map_initialized = 1;
    }

    VkDescriptorSet* cached = hashmap_get(&descriptor_map, buffers, buffer_count * sizeof(VKBUFFER));
    if (cached) {
        memcpy(program->buffers, buffers, buffer_count * sizeof(VKBUFFER));
        program->descriptor_set = *cached;
        return;
    }

    VkDescriptorSet* newSet = XMALLOC(sizeof(VkDescriptorSet));
    VkDescriptorSetAllocateInfo ai = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = ctx.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &program->descriptor_set_layout
    };
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &ai, newSet));

    VkDescriptorBufferInfo infos[buffer_count];
    VkWriteDescriptorSet   writes[buffer_count];
    uint32_t b;
    for (b = 0; b < buffer_count; ++b) {
        infos[b] = (VkDescriptorBufferInfo){
            .buffer = buffers[b].buffer,
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        writes[b] = (VkWriteDescriptorSet){
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = *newSet,
            .dstBinding      = program->buffer_indices[b],
            .descriptorCount = 1,
            .descriptorType  = program->buffer_types[b],
            .pBufferInfo     = &infos[b]
        };
    }
    vkUpdateDescriptorSets(ctx.device, buffer_count, writes, 0, NULL);
    VKBUFFER* buffers_heap = XMALLOC(buffer_count * sizeof(VKBUFFER));
    memcpy(buffers_heap, buffers, buffer_count * sizeof(VKBUFFER));
    hashmap_put(&descriptor_map, buffers_heap, buffer_count * sizeof(VKBUFFER), newSet);
    memcpy(program->buffers, buffers, buffer_count * sizeof(VKBUFFER));
    program->descriptor_set = *newSet;
}

void destroyProgram(VKCTX ctx, const char* shader_path){
    if (!program_map_initialized) {
        if (0 != hashmap_create(1, &program_map)) {
            printf("Error creating hashmap for program cache.\n");
            exit(1);
        }
        program_map_initialized = 1;
    }

    VKPROGRAM* program = hashmap_get(&program_map, shader_path, strlen(shader_path));
    if (!program) return;
    vkDestroyPipeline(ctx.device, program->pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, program->pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, program->descriptor_set_layout, NULL);
    free(program);
    hashmap_remove(&program_map, shader_path, strlen(shader_path));
}

#include <stdio.h>

// Define this to match your actual enum
const char* bindingLimitationToString(BindingLimitations lim) {
    switch(lim) {
        case READ_ONLY: return "READ_ONLY";
        case WRITE_ONLY: return "WRITE_ONLY";
        case READ_AND_WRITE: return "READ_AND_WRITE";
        default: return "UNKNOWN";
    }
}

void verifyVKPROGRAM(VKPROGRAM* prog) {
    if (!prog) {
        printf("VKPROGRAM is NULL\n");
        return;
    }

    printf("\n=== VKPROGRAM Verification ===\n");
    printf("Struct address: %p\n", (void*)prog);
    printf("descriptor_set_layout: %p\n", (void*)prog->descriptor_set_layout);
    printf("pipeline_layout:       %p\n", (void*)prog->pipeline_layout);
    printf("pipeline:              %p\n", (void*)prog->pipeline);
    printf("descriptor_set:        %p\n", (void*)prog->descriptor_set);
    printf("buffer_count:          %zu\n", prog->buffer_count);

    if (prog->buffer_count > MAX_BUFFERS) {
        printf("buffer_count (%zu) exceeds MAX_BUFFERS!\n", prog->buffer_count);
        return;
    }

    for (size_t i = 0; i < prog->buffer_count; i++) {
        printf("\n  Buffer [%zu]:\n", i);
        printf("    binding index:     %u\n", prog->buffer_indices[i]);
        printf("    descriptor type:   %u (7=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)\n", prog->buffer_types[i]);
        printf("    buffer handle:     %p\n", (void*)prog->buffers[i].buffer);
        printf("    binding limit:     %s\n", bindingLimitationToString(prog->binding_read_write_limitations[i]));
    }
    printf("================================\n\n");
}