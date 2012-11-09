
/*
  NOTE TO THE READER: The contents of the common.h header file are
  prepended here when the code is loaded.
 */

//#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
//#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
//#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
//#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

//#pragma OPENCL EXTENSION cl_ext_atomic_counters_32 : enable
#pragma OPENCL EXTENSION cl_amd_printf : enable


__kernel void empty_kernel() {
}

#define LATENCY_LOOP_CNT 1000000

__kernel void volatile_latency_kernel(volatile __global unsigned int *mem) {
  unsigned int offset = mem[0];
  for(unsigned int i = 0; i < LATENCY_LOOP_CNT; i++) {
    offset = mem[offset];
  }
  //keep the loop from being optimized away
  printf("done %d\n", offset);
}

__kernel void latency_kernel(__global unsigned int *mem) {
  unsigned int offset = mem[0];
  for(unsigned int i = 0; i < LATENCY_LOOP_CNT; i++) {
    offset = mem[offset];
  } 
  //keep the loop from being optimized away
  printf("done %d\n", offset);
}

__kernel void mem1gb_kernel(__global unsigned int *mem1,
                            __global unsigned int *mem2) {
  for(unsigned int i = 0; i < 128*1024*1024/sizeof(unsigned int); i++) {
    mem1[i] = mem2[i];
  } 
}
