/*  sparse_matrix_multiply.c  */
#include "../swarm.h"

/* ---------- tiny 4×4 CSR example ----------
   Matrix  A = | 1  2  0  0 |
               | 0  0  3  0 |
               | 4  0  0  5 |
               | 0  0  0  6 |

   CSR:
   row_start  = {0, 2, 3, 5, 6}
   col_index  = {0, 1, 2, 0, 3, 3}
   values     = {1, 2, 3, 4, 5, 6}
   -------------------------------------------*/

int main(){
    const char* spirv_path = "./shaders/compiled/sparse_matrix_multiply.spv";

    /* ---- host data ------------------------------------------------------- */
    const uint32_t N_ROWS  = 4;
    const uint32_t N_ELEMS = 6;

    float  h_inputs[4]      = {1.0f, 1.0f, 1.0f, 1.0f};   /* input vector x */
    uint32_t h_update[4]    = {0,1,2,3};                   /* rows to update */

    uint32_t h_start[5]     = {0, 2, 3, 5, 6};
    uint32_t h_toIdx[6]     = {0, 1, 2, 0, 3, 3};
    float    h_weights[6]   = {1, 2, 3, 4, 5, 6};

    float  h_outputs[4]     = {0};                         /* result y = A·x */

    /* ---- sizes ----------------------------------------------------------- */
    size_t inSz   = sizeof(h_inputs);
    size_t outSz  = sizeof(h_outputs);
    size_t upSz   = sizeof(h_update);
    size_t stSz   = sizeof(h_start);
    size_t idxSz  = sizeof(h_toIdx);
    size_t wSz    = sizeof(h_weights);

    /* ---- Vulkan set-up --------------------------------------------------- */
    printf("Create context...\n");
    VKCTX ctx = createVkContext();

    printf("Create program...\n");
    VKPROGRAM prog = createProgram(ctx, spirv_path);

    /* ---- GPU buffers ----------------------------------------------------- */
    VKBUFFER buf_inputs   = newBuffer(ctx, inSz,  BUF_GPU);
    VKBUFFER buf_outputs  = newBuffer(ctx, outSz, BUF_GPU);
    VKBUFFER buf_update   = newBuffer(ctx, upSz,  BUF_GPU);
    VKBUFFER buf_start    = newBuffer(ctx, stSz,  BUF_GPU);
    VKBUFFER buf_toIdx    = newBuffer(ctx, idxSz, BUF_GPU);
    VKBUFFER buf_weights  = newBuffer(ctx, wSz,   BUF_GPU);
    VKBUFFER buf_indirect = newBuffer(ctx, 3 * sizeof(uint32_t), BUF_INDIRECT);

    /* ---- CPU staging / read-back ---------------------------------------- */
    VKBUFFER cpu_stage    = newBuffer(ctx, 128, BUF_CPU);

    /* ---- upload host data to GPU -----------------------------------------
       Exactly the same pattern used in add.c: map, memcpy, unmap, copy.     */
    /* inputs */
    float* p = mapBuffer(ctx, cpu_stage);
    memcpy(p, h_inputs, inSz);
    unmapBuffer(ctx, cpu_stage);
    runCopyCommand(ctx, cpu_stage, buf_inputs, 0, 0, inSz);

    /* update indices */
    p = mapBuffer(ctx, cpu_stage);
    memcpy(p, h_update, upSz);
    unmapBuffer(ctx, cpu_stage);
    runCopyCommand(ctx, cpu_stage, buf_update, 0, 0, upSz);

    /* start_positions */
    p = mapBuffer(ctx, cpu_stage);
    memcpy(p, h_start, stSz);
    unmapBuffer(ctx, cpu_stage);
    runCopyCommand(ctx, cpu_stage, buf_start, 0, 0, stSz);

    /* to_indices */
    p = mapBuffer(ctx, cpu_stage);
    memcpy(p, h_toIdx, idxSz);
    unmapBuffer(ctx, cpu_stage);
    runCopyCommand(ctx, cpu_stage, buf_toIdx, 0, 0, idxSz);

    /* weights */
    p = mapBuffer(ctx, cpu_stage);
    memcpy(p, h_weights, wSz);
    unmapBuffer(ctx, cpu_stage);
    runCopyCommand(ctx, cpu_stage, buf_weights, 0, 0, wSz);

    uint32_t groups = (N_ROWS + 63) / 64;
    uint32_t* ip = mapBuffer(ctx, cpu_stage);
    ip[0] = groups;
    ip[1] = 1;
    ip[2] = 1;
    unmapBuffer(ctx, cpu_stage);
    runCopyCommand(ctx, cpu_stage, buf_indirect, 0, 0, buf_indirect.size);



    /* ---- bind to descriptor set ----------------------------------------- */
    VKBUFFER bufs[] = { buf_inputs, buf_outputs, buf_update,
                        buf_start,  buf_toIdx,   buf_weights };
    useBuffers(ctx, &prog, bufs, 6);

    /* ---- sanity check ---------------------------------------------------- */
    verifyVKPROGRAM(&prog);

    /* ---- dispatch -------------------------------------------------------- */
    printf("Launch %u work-groups...\n", groups);
    runComputeCommand(ctx, &prog, 1, buf_indirect);

    /* ---- read back ------------------------------------------------------- */
    runCopyCommand(ctx, buf_outputs, cpu_stage, 0, 0, outSz);

    /* ---- print ----------------------------------------------------------- */
    p = mapBuffer(ctx, cpu_stage);
    printf("y = A·x  -->  ");
    for(uint32_t i=0;i<N_ROWS;++i) printf("%.1f ", p[i]);
    printf("\n");
    unmapBuffer(ctx, cpu_stage);

    /* ---- clean-up -------------------------------------------------------- */
    destroyBuffer(ctx, buf_inputs);
    destroyBuffer(ctx, buf_outputs);
    destroyBuffer(ctx, buf_update);
    destroyBuffer(ctx, buf_start);
    destroyBuffer(ctx, buf_toIdx);
    destroyBuffer(ctx, buf_weights);
    destroyBuffer(ctx, buf_indirect);
    destroyBuffer(ctx, cpu_stage);
    destroyProgram(ctx, spirv_path);
    destroyVkContext(ctx);
    printf("Fin.\n");
    return 0;
}