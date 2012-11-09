
/*
  NOTE TO THE READER: The contents of the common.h header file are
  prepended here when the code is loaded.
 */

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

// TODO: Make queues length that is a multiple of work items.

// TODO: Rename queue to frontier!

// TODO: If there is a lot of space left in old_queue, just write to that one and do not go back?

__kernel void mark_step(volatile __global unsigned int *space,
                        __local unsigned int *prefix_buffer,
                        __global unsigned int *old_queue,
                        __global unsigned int *new_queue,
                        unsigned int base_ptr,
			__local unsigned int * scratch) {
  int id = get_global_id(0);

  // TODO: Maybe want to copy this into local space.
  __global unsigned int *in_queue = old_queue;
  __global unsigned int *out_queue = new_queue;

  int work_group_size = get_global_size(0);

  int qs = in_queue[0];
  int qe = in_queue[1];
  int length = min(work_group_size, qe - qs);

  int last_length = 0;

  if (id == 0) {
    out_queue[0] = 0;
    out_queue[1] = 0;
  }

  barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

  // TODO: Fix the use of last_length - it needs to be updated, otherwise it is stale. 
  while (length > 0 || last_length > 0) {
    int local_length = 0; // Number of references that need to be visited for this object.
    unsigned int addr = 0; // Offset into the heap structure that contains this object's data.
    unsigned int real_addr = 0; // Offset into the heap structure array.

    if (length > 0) {
      int out_qs = out_queue[0];
      int out_qe = out_queue[1];

      // Step 1: Mark the node and write number of references to be handled.
      if (id < length) {
        addr = in_queue[((qs+id) % QUEUE_SIZE) + 2]; // Translate from address into array offset.

        if (addr != NULL_REF) {
          real_addr = (addr - base_ptr) >> 2;

          // Header contains mark bit AND number of references. This gives us locality of access!
          // This should be VERY efficient because of efficient atomics in AMD Fusion.
          unsigned int header = atomic_or(&space[real_addr+HEADER_OFFSET], (uint)MARK_MASK);

          // Has it not been marked yet?
          if (!(header & MARK_MASK)) {
            // Add all its references to the queue.
            local_length = header & REFCOUNT_MASK;
          }
        }

        prefix_buffer[length+id] = local_length;
      }

      // Ensure consistent view of prefix_buffer before doing prefix sum.
      barrier(CLK_LOCAL_MEM_FENCE);

      // Step 2: Prefix sum over the number of nodes (to find global offsets).

      // TODO: Deferred handling of long entries.

      // Code based on naive algorithm presented in:
      // Parallel Prefix Sum (Scan) with CUDA, Mark Harris, April 2007, NVIDIA.

      // TODO: Try the more efficient implementation.

      int pout = 0, pin = 1;

      // Double buffering prefix-sum.
      for (int offset = 1; offset < length; offset *= 2) {
        if (id < length) {
          if (id >= offset) {
            prefix_buffer[pout*length+id] = prefix_buffer[pin*length+id] + prefix_buffer[pin*length+id-offset];
          } else {
            prefix_buffer[pout*length+id] = prefix_buffer[pin*length+id];
          }
        }

        pout = 1 - pout;
        pin = 1 - pin;

        barrier(CLK_LOCAL_MEM_FENCE);
      }

      if (id < length) {
        // Step 3: Copy entries to new locations.
        for (int j = 0; j < local_length; j++) {
          // Unroll for memory coalescing
          // TODO: Store as vectors.
          int offset = 0; // TODO: Shift everything by one and set first offset to 0.
          if (id != 0) offset = prefix_buffer[pin*length+id-1]; // TODO: Get rid of this by shifting by one.
          out_queue[((out_qe + offset + j) % QUEUE_SIZE)+2] = space[real_addr+REFS_OFFSET+j];
        } 
      }

      // TODO: Do these updates in different wavefronts?
      if (id == 0) {
        in_queue[0] = qs + length;
        out_queue[1] = out_qe + prefix_buffer[pin*length+length-1];
      }
    }

    barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

    __global unsigned int *tmp_queue = in_queue;
    in_queue = out_queue;
    out_queue = tmp_queue;

    qs = in_queue[0];
    qe = in_queue[1];
    last_length = length;
    length = min(work_group_size, qe - qs);

    barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
  }
}
