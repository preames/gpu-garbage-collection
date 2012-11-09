// CAUTION - This file must complile under C, C++, and OpenCL

#define VISITED 0x80000000
#define VISITED2 0x40000000

// Note: Queue size directly impacts amount of
// data copied to GPU (and thus kernel launch 
// overheads.)
#define QUEUE_SIZE 2000000
//#define QUEUE_SIZE 100

#define UINT_AT(x, offset) (*(uint32_t*)((intptr_t)x + offset))
#define PTR_AT(x, offset) (*(void**)((intptr_t)x + offset))

#define HEADER_OFFSET 1
#define REFS_OFFSET 2

#define MARK_MASK 0x80000000
#define REFCOUNT_MASK 0x0000ffff

#define NULL_REF 0x00000000

#define DATA_SIZE 256
// Number of queue entries handled by one work group at once.
#define WORK_SIZE 256
#define BUFFER_SIZE 1000

#define WAVEFRONT_SIZE 64

// When below or at this degree of parallelism, switch back to CPU.
// Must be larger or equal than 0.
//#define CPU_FALLBACK 0

// Minimum number of iterations to be performed on GPU before going
// back to the GPU.
#define MIN_GPU_ITERATIONS 5

// When above this degree of parallelism, switch to GPU.
// TODO: May want to have minimum number of GPU/CPU steps.
// #define GPU_FALLBACK 1000 Note: This is now a configurable parameter via gpugc_cpu_fallback

// For tests from gpugc.c
// #define NULL_REF 0x3fffffff
