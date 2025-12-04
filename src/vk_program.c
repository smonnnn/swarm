#include "vk_program.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool key_eq(const uint32_t *a, const uint32_t *b, uint32_t n) {
    return memcmp(a, b, n * sizeof(*a)) == 0;
}

typedef struct {
    uint32_t               *types;
    uint32_t                count;
    VkDescriptorSetLayout   layout;
} LayoutEntry;

static LayoutEntry *layout_cache = NULL;
static size_t       layout_cache_count = 0;
VkDescriptorSetLayout getOrCreateDescriptorSetLayout(VKCTX ctx, ShaderInfo s){
    for (size_t i = 0; i < layout_cache_count; ++i)
        if (layout_cache[i].count == s.buffer_count &&
            key_eq(layout_cache[i].types, s.buffer_types, s.buffer_count))
            return layout_cache[i].layout;

    VkDescriptorSetLayoutBinding bindings[s.buffer_count];
    for (uint32_t i = 0; i < s.buffer_count; ++i) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding         = s.buffer_indices[i],   // slot index
            .descriptorType  = s.buffer_types[i],     // SSBO, UBO, image, …
            .descriptorCount = 1,                     // array size (1 = single)
            .stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        };
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = s.buffer_count,
        .pBindings    = bindings,
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &info, NULL, &layout));
        XREALLOC(layout_cache, (layout_cache_count + 1) * sizeof(*layout_cache));
    layout_cache[layout_cache_count] = (LayoutEntry){
        .types  = XMALLOC(s.buffer_count * sizeof(VkDescriptorType)),
        .count  = s.buffer_count,
        .layout = layout
    };
    memcpy(layout_cache[layout_cache_count].types, s.buffer_types, s.buffer_count * sizeof(VkDescriptorType));
    ++layout_cache_count;
    return layout;
}

typedef struct {
    VkDescriptorSetLayout setLayout;
    VkPipelineLayout      layout;
} PloEntry;

static PloEntry *plo_cache     = NULL;
static size_t    plo_cache_cnt = 0;
VkPipelineLayout getOrCreatePipelineLayout(VKCTX ctx, VkDescriptorSetLayout setLayout){
    for (size_t i = 0; i < plo_cache_cnt; ++i)
        if (plo_cache[i].setLayout == setLayout)
            return plo_cache[i].layout;

    VkPipelineLayoutCreateInfo ci = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &setLayout,
    };
    VkPipelineLayout plo;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &ci, NULL, &plo));

    XREALLOC(plo_cache, (plo_cache_cnt + 1) * sizeof(*plo_cache));
    plo_cache[plo_cache_cnt++] = (PloEntry){ setLayout, plo };
    return plo;
}

static inline uint32_t word(const uint32_t* p, size_t off) { return p[off]; }
static inline uint32_t opcode(const uint32_t* p, size_t off) { return p[off] & 0xFFFFu; }
static inline uint32_t length(const uint32_t* p, size_t off) { return p[off] >> 16; }

static VkDescriptorType classify(int storageClass, int dataOp, const uint32_t* code, size_t dataOff){
    (void)code; (void)dataOff;   /* unused in this minimal version */

    switch (storageClass) {
    case 9:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;              /* PushConstant */
    case 12: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;              /* Uniform block   */
    case 11: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;              /* StorageBuffer   */
    case 2:  {                                                      /* UniformConstant */
        if (dataOp == 25) {                                         /* OpTypeImage */
            /* sampled flag is word 6 of OpTypeImage */
            uint32_t sampled = (dataOff < 64) ? 1 : 0; 
            return sampled ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                           : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        if (dataOp == 26) return VK_DESCRIPTOR_TYPE_SAMPLER;
        if (dataOp == 27) return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;
    }
    }
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
}

ShaderInfo readShader(const char* shader_path){
    ShaderInfo s = {0};

    /* ---- load file ---------------------------------------------- */
    FILE* f = fopen(shader_path, "rb");
    if (!f) return s;
    fseek(f, 0, SEEK_END);
    size_t byteLen = ftell(f);
    rewind(f);
    uint32_t* code = (uint32_t*)XMALLOC(byteLen);
    fread(code, 1, byteLen, f);
    if (byteLen < 20 || code[0] != 0x07230203) {
        fclose(f);
        printf("Attempting to read malformed spirv file.\n");
        exit(0);
    }
    fclose(f);
    s.spirv_bytecode        = code;
    s.spirv_bytecode_length = byteLen;
    size_t wordCount = byteLen / 4;
    uint32_t bound   = code[3];

    /* ---- exact-size side tables --------------------------------- */
    int id2binding[512] = {0};
    int id2storage[512] = {0};
    int id2dataOp[512]  = {0};   /* opcode of pointee */
    int touched[512]    = {0};   /* binding→usage */
    int ptr2base[512] = {0};

    for (uint32_t i = 0; i < bound; ++i) id2binding[i] = -1;

    /* ---- PASS 1 : types, decorations, variables --------------- */
    size_t off = 5;
    while (off < wordCount) {
        uint32_t len = length(code, off);
        uint32_t op  = opcode(code, off);

        /* OpTypePointer */
        if (op == 32) {
            uint32_t id  = word(code, off + 1);
            uint32_t sc  = word(code, off + 2);
            uint32_t pte = word(code, off + 3);
            id2storage[id] = sc;
            id2dataOp[id]  = opcode(code, pte * 0); /* will refine below */
        }
        /* OpTypeImage / OpTypeSampler / … */
        if (op == 25 || op == 26 || op == 27) {
            uint32_t id = word(code, off + 1);
            id2dataOp[id] = op;
        }

        /* OpDecorate … Binding */
        if (op == 71 && word(code, off + 2) == 33) {
            uint32_t target = word(code, off + 1);
            uint32_t bind   = word(code, off + 3);
            if (target < bound && bind < 256) id2binding[target] = bind;
        }

        /* OpVariable */
        if (op == 59) {
            uint32_t varId   = word(code, off + 2);
            uint32_t ptrType = word(code, off + 1);
            id2storage[varId] = id2storage[ptrType];
            id2dataOp[varId]  = id2dataOp[ptrType];
        }
        if (op == 65) {
            uint32_t result = word(code, off + 2);
            uint32_t base   = word(code, off + 3);
            ptr2base[result] = base;
        }

        off += len;
    }

    /* ---- PASS 2 : mark reads / writes -------------------------- */
    off = 5;
    while (off < wordCount) {
        uint32_t len = length(code, off);
        uint32_t op  = opcode(code, off);

        if (op == 61 || op == 62) {               /* OpLoad or OpStore */
            uint32_t ptr = word(code, off + (op == 61 ? 3 : 1));

            /* follow access-chain table to base variable */
            while (ptr && ptr2base[ptr]) ptr = ptr2base[ptr];

            /* if base is decorated, mark the binding */
            int b = (ptr < 512) ? id2binding[ptr] : -1;
            if (b >= 0) touched[b] |= (op == 61 ? 1 : 2);
        }
        off += len;
    }
    const char *entry = NULL;
    off = 5;
    while (off < wordCount) {
        uint32_t len = length(code, off);
        uint32_t op  = opcode(code, off);
        if (op == 71 && word(code, off + 2) == 71) { /* OpEntryPoint */
            entry = (const char *)&code[off + 3];    /* word after execution model */
            break;
        }
        off += len;
    }
    if (!entry) entry = "main";      /* sane default */
    s.entrypoint = XMALLOC(strlen(entry) + 1);
    strcpy(s.entrypoint, entry);

    printf("---- all decorated IDs  ----\n");
    for (uint32_t id = 0; id < bound; ++id)
        if (id2binding[id] != -1)
            printf("  id %3u  binding=%d\n", id, id2binding[id]);

    printf("---- all pointer IDs used by OpLoad/OpStore ----\n");
    off = 5;
    while (off < wordCount) {
        uint32_t len = length(code, off);
        uint32_t op  = opcode(code, off);
        if (op == 61 || op == 62) {
            uint32_t ptr = word(code, off + (op == 61 ? 3 : 1));
            printf("  %s  pointer_id=%u\n", op == 61 ? "load" : "store", ptr);
        }
        off += len;
    }

    /* ---- build output arrays ----------------------------------- */
    size_t count = 0;
    for (int b = 0; b < 256; ++b) if (touched[b]) ++count;

    s.buffer_indices                    = (uint32_t*)           XMALLOC(count * sizeof(uint32_t));
    s.binding_read_write_limitations    = (BindingLimitations*) XMALLOC(count * sizeof(BindingLimitations));
    s.buffer_types                      = (VkDescriptorType*)   XMALLOC(count * sizeof(VkDescriptorType));
    s.buffer_count = 0;

    for (int b = 0; b < 256; ++b) {
        if (!touched[b]) continue;
        uint32_t var = 0;
        /* find *any* variable with this binding so we can classify */
        for (uint32_t id = 0; id < bound; ++id)
            if (id2binding[id] == b) { var = id; break; }

        s.buffer_indices[s.buffer_count] = b;
        s.binding_read_write_limitations[s.buffer_count] = (touched[b] == 1) ? READ_ONLY : (touched[b] == 2) ? WRITE_ONLY : READ_AND_WRITE;
        s.buffer_types[s.buffer_count] = classify(id2storage[var], id2dataOp[var], code, 0);
        ++s.buffer_count;
    }

    printf("Read %u buffers from SPIRV bytecode\n", s.buffer_count);
    for (uint32_t i = 0; i < s.buffer_count; ++i) {
        printf("  [%u]  binding=%u  type=%d  limit=%d\n",
            i,
            s.buffer_indices[i],
            (int)s.buffer_types[i],
            (int)s.binding_read_write_limitations[i]);
    }

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

VKPROGRAM createProgram(VKCTX ctx, const char* shader_path){
    VKPROGRAM program = {0};
    ShaderInfo shader_info = readShader(shader_path);
    program.buffer_indices = shader_info.buffer_indices;
    program.buffer_types = shader_info.buffer_types;
    program.binding_read_write_limitations = shader_info.binding_read_write_limitations;
    program.descriptor_set_layout = getOrCreateDescriptorSetLayout(ctx, shader_info);
    program.pipeline_layout = getOrCreatePipelineLayout(ctx, program.descriptor_set_layout);
    program.pipeline = createPipeline(ctx, program.pipeline_layout, shader_info);
    return program;
}

#define DECAY_MAX      127          /* 7-bit saturating counter */
#define DECAY_INTERVAL 64           /* decay once every 64 cache lookups */

static uint32_t accessCount = 0;    /* fine-grain counter */
static inline uint8_t sat(int v) { return (v < 0) ? 0 : (v > DECAY_MAX ? DECAY_MAX : v); }

typedef struct{
    VKBUFFER* buffers;
    uint32_t buffer_count;
    VkDescriptorSet descriptor_set;
    bool in_use;
    uint8_t usage;
} DescEntry;

DescEntry* desc_cache = NULL;
static size_t desc_cache_cnt = 0;
static bool allocateDescriptorSet(VkDevice dev, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkDescriptorSet *out){
    VkDescriptorSetAllocateInfo ai = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = pool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &layout
    };
    return vkAllocateDescriptorSets(dev, &ai, out) == VK_SUCCESS;
}

//Note: Buffers are in order of bindings so their index in the array corresponds to their binding idx.
void useBuffers(VKCTX ctx, VKPROGRAM *prog, VKBUFFER *buffers, uint32_t buf_count){
    /* ---- 1.  LRU decay ---------------------------------------- */
    if (++accessCount >= DECAY_INTERVAL) {
        accessCount = 0;
        for (size_t i = 0; i < desc_cache_cnt; ++i)
            desc_cache[i].usage >>= 1;
    }

    /* ---- 2.  fast hit ----------------------------------------- */
    for (size_t i = 0; i < desc_cache_cnt; ++i) {
        DescEntry *e = &desc_cache[i];
        if (e->buffer_count == buf_count &&
            memcmp(e->buffers, buffers, buf_count * sizeof(VKBUFFER)) == 0)
        {
            e->usage = DECAY_MAX;
            prog->descriptor_set = e->descriptor_set;
            /* update client copy */
            free(prog->buffers);
            prog->buffers     = XMALLOC(buf_count * sizeof(VKBUFFER));
            prog->buffer_count = buf_count;
            memcpy(prog->buffers, buffers, buf_count * sizeof(VKBUFFER));
            return;
        }
    }

    /* ---- 3.  choose victim ------------------------------------ */
    int victim = -1;
    uint8_t min_usage = UINT8_MAX;
    for (size_t i = 0; i < desc_cache_cnt; ++i) {
        DescEntry *e = &desc_cache[i];
        if (!e->in_use && e->usage < min_usage) {
            min_usage = e->usage;
            victim    = (int)i;
        }
    }

    /* ---- 4.  allocate descriptor set -------------------------- */
    VkDescriptorSet newSet;
    bool ok = allocateDescriptorSet(ctx.device, ctx.descriptor_pool, prog->descriptor_set_layout, &newSet);
    if (!ok && victim == -1) {
        fprintf(stderr, "Out of descriptor sets and no victim!\n");
        exit(1);                       /* or return false */
    }

    if (!ok) {                         /* re-use victim set */
        newSet = desc_cache[victim].descriptor_set;
        free(desc_cache[victim].buffers);
    } else if (victim == -1) {         /* need to grow cache */
        XREALLOC(desc_cache, (desc_cache_cnt + 1) * sizeof(*desc_cache));

        victim = (int)desc_cache_cnt++;
    } else {                           /* overwrite victim slot */
        free(desc_cache[victim].buffers);
    }

    /* ---- 5.  fill new cache entry ----------------------------- */
    DescEntry *slot = &desc_cache[victim];
    slot->buffers      = XMALLOC(buf_count * sizeof(VKBUFFER));
    slot->buffer_count = buf_count;
    slot->descriptor_set = newSet;
    slot->in_use       = false;
    slot->usage        = DECAY_MAX;
    memcpy(slot->buffers, buffers, buf_count * sizeof(VKBUFFER));

    /* ---- 6.  write descriptors -------------------------------- */
    VkDescriptorBufferInfo infos[buf_count];
    VkWriteDescriptorSet   writes[buf_count];
    for (uint32_t b = 0; b < buf_count; ++b) {
        infos[b] = (VkDescriptorBufferInfo){
            .buffer = buffers[b].buffer,
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        writes[b] = (VkWriteDescriptorSet){
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = newSet,
            .dstBinding      = prog->buffer_indices[b],
            .descriptorCount = 1,
            .descriptorType  = prog->buffer_types[b],
            .pBufferInfo     = &infos[b]
        };
    }
    vkUpdateDescriptorSets(ctx.device, buf_count, writes, 0, NULL);

    /* ---- 7.  publish to program ------------------------------- */
    prog->descriptor_set = newSet;
    free(prog->buffers);
    prog->buffers       = XMALLOC(buf_count * sizeof(VKBUFFER));
    prog->buffer_count  = buf_count;
    memcpy(prog->buffers, buffers, buf_count * sizeof(VKBUFFER));
}

void destroyProgram(VKCTX ctx, VKPROGRAM* program){
    vkDestroyPipeline(ctx.device, program->pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, program->pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, program->descriptor_set_layout, NULL);
    free(program->buffer_types);
    free(program->buffer_indices);
    free(program->buffers);
    free(program->binding_read_write_limitations);
    memset(program, 0, sizeof(*program));
}