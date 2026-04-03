/*  sparse_chunked.c  */
#include "../../swarm.h"

#define CHUNK_SIZE 16 //Must match shader definition!
//Staging buffer should be a static thing.

typedef struct {
    uint32_t to[CHUNK_SIZE];
    float weights[CHUNK_SIZE];
} Chunk;

typedef struct {
    uint32_t  chunk_count;
    uint32_t* from;
    uint32_t* offsets;
    Chunk* chunks;
} Matrix;

typedef struct {
    uint32_t  chunk_count;
    VKBUFFER from;
    VKBUFFER chunks;
} GPU_Matrix;

bool addChunk(Matrix* matrix, uint32_t from, uint32_t to, float value) {
    uint32_t idx = matrix->chunk_count;
    // Allocate temporary arrays first
    Chunk* new_chunks = realloc(matrix->chunks, (idx + 1) * sizeof(Chunk));
    if (!new_chunks) return false;
    uint32_t* new_from = realloc(matrix->from, (idx + 1) * sizeof(uint32_t));
    if (!new_from) { free(new_chunks); return false; }
    uint32_t* new_offsets = realloc(matrix->offsets, (idx + 1) * sizeof(uint32_t));
    if (!new_offsets) { free(new_chunks); free(new_from); return false; }

    // Initialize the new chunk
    memset(&new_chunks[idx], 0, sizeof(Chunk));  // zeros both arrays
    new_chunks[idx].to[0] = to;
    new_chunks[idx].weights[0] = value;
    new_from[idx] = from;
    new_offsets[idx] = 1;

    // Commit
    matrix->chunks = new_chunks;
    matrix->from = new_from;
    matrix->offsets = new_offsets;
    matrix->chunk_count++;
    return true;
}

void setWeight(Matrix* matrix, uint32_t from, uint32_t to, float value) {
    // First, try to update existing entry
    for (uint32_t i = 0; i < matrix->chunk_count; i++) {
        if (matrix->from[i] == from) {
            Chunk* c = &matrix->chunks[i];
            for (uint32_t j = 0; j < matrix->offsets[i]; j++) {
                if (c->to[j] == to) {
                    c->weights[j] = value;
                    return;
                }
            }
        }
    }

    // Not found: add new entry
    for (uint32_t i = 0; i < matrix->chunk_count; i++) {
        if (matrix->from[i] == from && matrix->offsets[i] < CHUNK_SIZE) {
            uint32_t p = matrix->offsets[i];
            matrix->chunks[i].to[p] = to;
            matrix->chunks[i].weights[p] = value;
            matrix->offsets[i]++;
            return;
        }
    }
    // No space in any chunk of this row (or row doesn't exist yet)
    addChunk(matrix, from, to, value);
}

GPU_Matrix* toGPU_Matrix(VKCTX ctx, Matrix* matrix){
    uint32_t cc = matrix->chunk_count;

    GPU_Matrix* gpu_matrix = malloc(sizeof(GPU_Matrix));
    gpu_matrix->chunk_count = cc;
    gpu_matrix->from = newBuffer(ctx, cc * sizeof(uint32_t), BUF_GPU);
    gpu_matrix->chunks = newBuffer(ctx, cc * sizeof(Chunk), BUF_GPU);
    VKBUFFER stage = newBuffer(ctx, cc * sizeof(Chunk), BUF_CPU);

    Chunk* p = mapBuffer(ctx, stage);
    memcpy(p, matrix->chunks, cc * sizeof(Chunk));
    unmapBuffer(ctx, stage);
    copyBufferSlow(ctx, stage, gpu_matrix->chunks, 0, 0, cc * sizeof(Chunk));

    p = mapBuffer(ctx, stage);
    memcpy(p, matrix->from, cc * sizeof(uint32_t));
    unmapBuffer(ctx, stage);
    copyBufferSlow(ctx, stage, gpu_matrix->from, 0, 0, cc * sizeof(uint32_t));
    destroyBuffer(ctx, stage);
    return gpu_matrix;
}

Matrix fromGPU_Matrix(VKCTX ctx, GPU_Matrix* gpu_matrix){
    uint32_t cc = gpu_matrix->chunk_count;
    Matrix matrix = {0};
    matrix.chunk_count = cc;
    matrix.from = malloc(cc * sizeof(uint32_t));
    matrix.chunks = malloc(cc * sizeof(Chunk));
    VKBUFFER stage = newBuffer(ctx, cc * sizeof(Chunk), BUF_CPU);

    copyBufferSlow(ctx, gpu_matrix->from, stage, 0, 0, cc * sizeof(uint32_t));
    uint32_t* p = mapBuffer(ctx, stage);
    memcpy(matrix.from, p, cc * sizeof(uint32_t));
    unmapBuffer(ctx, stage);

    copyBufferSlow(ctx, gpu_matrix->chunks, stage, 0, 0, cc * sizeof(Chunk));
    p = mapBuffer(ctx, stage);
    memcpy(matrix.chunks, p, cc * sizeof(Chunk));
    unmapBuffer(ctx, stage);
    destroyBuffer(ctx, stage);
    return matrix;
}

void printMatrix(const char* name, Matrix* matrix) {
    printf("=== Matrix: %s ===\n", name);
    printf("Chunk count: %u\n", matrix->chunk_count);
    
    for (uint32_t i = 0; i < matrix->chunk_count; i++) {
        printf("Chunk %u (from=%u, used=%u/%d):\n", 
               i, matrix->from[i], matrix->offsets[i], CHUNK_SIZE);
        
        Chunk* c = &matrix->chunks[i];
        for (uint32_t j = 0; j < CHUNK_SIZE; j++) {
            if (c->weights[j] != 0.0f) {
                printf("  to[%u]=%u, weight=%.2f\n", j, c->to[j], c->weights[j]);
            }
        }
    }
    printf("==================\n");
}

void main(){
    const char* mm_spirv_path = "./chunked_sparse_matrix_multiply.spirv";
    const char* filter_spirv_path = "./csmm_filter.spirv";
    VKCTX ctx = createVkContext();
    VKPROGRAM program_mm = createProgram(ctx, mm_spirv_path);
    VKPROGRAM program_filter = createProgram(ctx, filter_spirv_path);

    Matrix cmat = {0};
    setWeight(&cmat, 0, 0, 5.0f);
    setWeight(&cmat, 1, 1, 2.0f);
    setWeight(&cmat, 2, 2, 1.0f);
    setWeight(&cmat, 3, 3, 1.0f);

    setWeight(&cmat, 0, 1, 1.0f);
    //setWeight(&cmat, 1, 2, 1.0f);
    //setWeight(&cmat, 2, 3, 1.0f);
    //setWeight(&cmat, 3, 4, 1.0f);

    GPU_Matrix* gmat = toGPU_Matrix(ctx, &cmat);
    free(cmat.chunks);
    free(cmat.from);

    VKBUFFER input = newBuffer(ctx, 4 * sizeof(float), BUF_GPU);
    VKBUFFER output = newBuffer(ctx, 4 * sizeof(float), BUF_GPU);
    VKBUFFER active = newBuffer(ctx, 4 * sizeof(uint32_t), BUF_GPU);
    VKBUFFER counter = newBuffer(ctx, 1 * sizeof(uint32_t), BUF_GPU);
    VKBUFFER indirect = newBuffer(ctx, 3 * sizeof(uint32_t), BUF_INDIRECT);

    //set values.
    float input_data[] = {1.0, 1.0, 1.0, 1.0};
    VKBUFFER stage = newBuffer(ctx, 4 * sizeof(float), BUF_CPU);
    void* p = mapBuffer(ctx, stage);
    memcpy(p, input_data, 4 * sizeof(float));
    unmapBuffer(ctx, stage);
    copyBufferSlow(ctx, stage, input, 0, 0, 4 * sizeof(float));
    
    uint32_t indirect_data[] = {
        (4 + 63) / 64,  // num_groups_x = ceil(4/64) = 1
        1,              // num_groups_y
        1               // num_groups_z
    };
    p = mapBuffer(ctx, stage);
    memcpy(p, indirect_data, 3 * sizeof(uint32_t));
    unmapBuffer(ctx, stage);
    copyBufferSlow(ctx, stage, indirect, 0, 0, 3 * sizeof(uint32_t));

    uint32_t active_data[] = {0, 1, 2, 3};  // Process all 4 chunks
    p = mapBuffer(ctx, stage);
    memcpy(p, active_data, 4 * sizeof(uint32_t));
    unmapBuffer(ctx, stage);
    copyBufferSlow(ctx, stage, active, 0, 0, 4 * sizeof(uint32_t));
    
    //run program
    VKBUFFER buffers[] = {input, output, gmat->from, active, gmat->chunks, counter};
    useBuffers(ctx, &program_mm, buffers, 6);
    VkCommandBuffer cmd = startCommand(ctx);
    runProgram(ctx, cmd, program_mm, indirect);
    submitCommand(ctx, cmd);

    float output_data[4];
    copyBufferSlow(ctx, output, stage, 0, 0, 4*sizeof(float));
    p = mapBuffer(ctx, stage);
    memcpy(output_data, p, 4 * sizeof(float));
    unmapBuffer(ctx, stage);
    
    printf("Output values: ");
    for (int i = 0; i < 4; i++) {
        printf("%.2f ", output_data[i]);
    }
    printf("\n");

    destroyBuffer(ctx, stage);
    destroyBuffer(ctx, input);
    destroyBuffer(ctx, output);
    destroyBuffer(ctx, active);
    destroyBuffer(ctx, counter);
    destroyBuffer(ctx, indirect);
    destroyBuffer(ctx, gmat->from);
    destroyBuffer(ctx, gmat->chunks);
    free(gmat);
    
    destroyProgram(ctx, mm_spirv_path);
    destroyProgram(ctx, filter_spirv_path);
    destroyVkContext(ctx);
}   