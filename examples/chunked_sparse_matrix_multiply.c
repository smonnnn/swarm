/*  sparse_chunked.c  */
#include "../swarm.h"

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

void addChunk(Matrix* matrix, uint32_t from, uint32_t to, float value){
    Chunk* tmpC = realloc(matrix->chunks, (matrix->chunk_count + 1) * sizeof(Chunk));
    memset(tmpC[matrix->chunk_count - 1].weights, 0.0f, CHUNK_SIZE * sizeof(float));
    tmpC[matrix->chunk_count - 1].to[0] = to;
    tmpC[matrix->chunk_count - 1].weights[0] = value;
    matrix->chunks = tmpC;

    uint32_t* tmpI = realloc(matrix->from, (matrix->chunk_count + 1) * sizeof(uint32_t));
    tmpI[matrix->chunk_count - 1] = from;
    matrix->from = tmpI;
    matrix->chunk_count++;

    uint32_t* tmpI = realloc(matrix->offsets, (matrix->chunk_count + 1) * sizeof(uint32_t));
    matrix->offsets[matrix->chunk_count - 1] = 1;
}

//Helper for setting weights in a more interpretable manner. Weights should be updated on the GPU for actual applications.
void setWeight(Matrix* matrix, uint32_t from, uint32_t to, float value){
    for(int i = 0; i < matrix->chunk_count; i++){
        if(matrix->from[i] == from){
            uint32_t p = matrix->offsets[i];
            if(p < CHUNK_SIZE){
                matrix->chunks[i].weights[p] = value;
                matrix->chunks[i].to[p] = to;
                matrix->offsets[i]++;
                return;
            }
        }
    }
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

    uint32_t* p = mapBuffer(ctx, stage);
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
    memcpy(p, matrix.from, cc * sizeof(uint32_t));

    copyBufferSlow(ctx, gpu_matrix->chunks, stage, 0, 0, cc * sizeof(Chunk));
    uint32_t* p = mapBuffer(ctx, stage);
    memcpy(p, matrix.chunks, cc * sizeof(Chunk));
    destroyBuffer(ctx, stage);
    return matrix;
}

void main(){
    const char* mm_spirv_path = "./shaders/compiled/chunked_sparse_matrix_multiply.spv";
    const char* filter_spirv_path = "./shaders/compiled/chunked_sparse_matrix_multiply.spv";
    VKCTX ctx = createVkContext();
    VKPROGRAM program_mm = createProgram(ctx, mm_spirv_path);
    VKPROGRAM program_filter = createProgram(ctx, filter_spirv_path);

    Matrix cmat = {0};
    setWeight(&cmat, 0, 0, 1.0f);
    setWeight(&cmat, 1, 1, 1.0f);
    setWeight(&cmat, 2, 2, 1.0f);
    setWeight(&cmat, 3, 3, 1.0f);

    setWeight(&cmat, 0, 1, 1.0f);
    setWeight(&cmat, 1, 2, 1.0f);
    setWeight(&cmat, 2, 3, 1.0f);
    setWeight(&cmat, 3, 4, 1.0f);

    GPU_Matrix* gmat = toGPU_Matrix(ctx, &cmat);
    free(cmat.chunks);
    free(cmat.from);

    VKBUFFER input = newBuffer(ctx, 4 * sizeof(float), BUF_GPU);
    VKBUFFER output = newBuffer(ctx, 4 * sizeof(float), BUF_GPU);
    VKBUFFER active = newBuffer(ctx, 4 * sizeof(float), BUF_GPU);
    VKBUFFER counter = newBuffer(ctx, 1 * sizeof(uint32_t), BUF_GPU);
    VKBUFFER indirect = newBuffer(ctx, 3 * sizeof(uint32_t), BUF_INDIRECT);

    //set values.
    float input_data[] = {1.0, 1.0, 1.0, 1.0};
    VKBUFFER stage = newBuffer(ctx, 4 * sizeof(float), BUF_CPU);
    uint32_t* p = mapBuffer(ctx, stage);
    memcpy(p, input_data, 4 * sizeof(float));
    unmapBuffer(ctx, stage);
    copyBufferSlow(ctx, stage, input, 0, 0, 4 * sizeof(float));
    destroyBuffer(ctx, stage);

    //run program
    VKBUFFER buffers[] = {input, output, gmat->from, active, gmat->chunks, counter};
    useBuffers(ctx, &program_mm, buffers, 6);
    VkCommandBuffer cmd = startCommand(ctx);
    runProgram(ctx, cmd, program_mm, indirect);
    submitCommand(ctx, cmd);
}   