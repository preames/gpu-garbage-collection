
/*
  NOTE TO THE READER: The contents of the common.h header file are
  prepended here when the code is loaded.

//This are defined by the driver
//#define CPU_FALLBACK 0 to 256
//#define USE_VECTORS 0 or 1
//#define USE_PREFIX_SUM 0 or 1
//#define USE_DIVERGENCE 0 or 1
//#define USE_BOTH_COMPUTE 0 or 1
//#define USE_LOAD_BALENCE 0 or 1 (only if both compute enabled)
 */

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

#pragma OPENCL EXTENSION cl_ext_atomic_counters_32 : enable

/* IDEAS FOR OPTIMIZATION (TODO):
   - Examine memory banking
   - Separate the workgroups so they don't have to run in lockstep
   - Sort the buffer (for uniqueness and memory access order)
   - Cutoffs
*/

#define DIVERGENCE_CUTOFF 64

//Is the multiple workgroup locking enabled?
#define COUNTER_LOCK_ENABLED 0

#define QUEUE_LOCK \
    if( get_local_id(0) == 0) { \
      while(true) { \
        const uint old = atomic_inc(counter); \
        if( old == *counter_copy ) { \
          /*lock aquired*/           \
          break; \
        } \
        while( old > *counter_copy ) {} \
      } \
    } \

#define QUEUE_UNLOCK \
  if( get_local_id(0) == 0) { \
    const uint old = atomic_inc(counter); \
    (*counter_copy) = old + 1; \
  } \


inline unsigned queue_length(volatile __global unsigned int* q) {
  const unsigned qs = q[0];
  const unsigned qe = q[1];
  //handle the wrap around case
  return ((qe + QUEUE_SIZE - qs) % QUEUE_SIZE);
}


/// This sorting algorithm is suitable for sorting small blocks using a single
/// workgroup (as modified).  The major catch is that you must know the size
/// of the input and have allocated sufficient scratch space.
/// Code from: http://www.bealto.com/gpu-sorting_parallel-bitonic-local.html
/// Performance measured at 191 MKey/s w/1 work group
inline void ParallelBitonic_Local(int wg, //number of elements to sort MAX of get_local_size(0)
				  __global const unsigned * in,
				  __global unsigned * out,
				  __local unsigned * aux) {
  int i = get_local_id(0); // index in workgroup

  int init = wg;
  if( wg < get_local_size(0) ) 
    wg = 1 << (32 - clz(wg)); // Round up to next power of 2.

  // Move IN, OUT to block start
  //int offset = get_group_id(0) * wg;
  //in += offset; out += offset;

  // Load block in AUX[WG]
  if( i < init ) {
    aux[i] = in[i];
  } else {
    //make sure it sorts high
    aux[i] = 0xFFFFFFFF;
  }
  barrier(CLK_LOCAL_MEM_FENCE); // make sure AUX is entirely up to date

  // Loop on sorted sequence length
  for (int length=1;length<wg;length<<=1)
    {
      bool direction = ((i & (length<<1)) != 0); // direction of sort: 0=asc, 1=desc
      // Loop on comparison distance (between keys)
      for (int inc=length;inc>0;inc>>=1)
	{
	  int j = i ^ inc; // sibling to compare
	  unsigned iData = aux[i];
	  uint iKey = iData; //getKey(iData);
	  unsigned jData = aux[j];
	  uint jKey = jData; //getKey(jData);
	  bool smaller = (jKey < iKey) || ( jKey == iKey && j < i );
	  bool swap = smaller ^ (j < i) ^ direction;
	  barrier(CLK_LOCAL_MEM_FENCE);
	  aux[i] = (swap)?jData:iData;
	  barrier(CLK_LOCAL_MEM_FENCE);
	}
    }

  // Write output
  if(i < init)
    out[i] = aux[i];
}




typedef struct range_in_queue_t {
  unsigned int begin;
  unsigned int length;
} range_in_queue;

//Returns the start and end of a block
// NOTE: The queue must be locked when this is called!
// IMPORTANT: This function leaves the block in the previous
// location in the queue, but updates the queue not to know about
// it.  As such, we can't add any block to the queue until _all_
// workgroups have finished processing their blocks.
inline range_in_queue get_region_from_input(volatile __global unsigned int* queue) {
  unsigned int work_group_size = get_local_size(0);

  unsigned int qs = queue[0];
  unsigned int qe = queue[1];
  //ex: 2,3,100->1, 99,1,100->2
  unsigned int length = min(work_group_size,  queue_length(queue));
  unsigned int newqs = (qs + length) % QUEUE_SIZE;

  queue[0] = newqs;

  range_in_queue rval;
  rval.begin = qs;
  rval.length = length;
  return rval;
}
// Allocates a region in the queue for us to copy into
// NOTE: The queue must be locked when this is called!
// NOTE: The queue must be locked until the data has been copied in
inline range_in_queue allocate_region_in_output(volatile __global unsigned int* queue,
                                                int space_needed) {
  unsigned int qs = queue[0];
  unsigned int qe = queue[1];
  unsigned int newqe = (qe + space_needed) % QUEUE_SIZE;
  //TODO: bounds checks

  //update the queue
  queue[1] = newqe;

  range_in_queue rval;
  rval.begin = qe;
  rval.length = space_needed;
  return rval;
}

/// Compute a prefix sum on the data in prefix_buffer (which is initialized to prefix_len) using
/// a single work group of threads (but many wavefronts).  Note: prefix_len must be less than get_local_size(0)!
inline void compute_prefix_sum(__local unsigned int *prefix_buffer,/*input/output*/
                               unsigned int prefix_len /*length of the populated section of the prefix_buffer*/) {
  int id = get_local_id(0);
  // if (i != 10) {

  int offset = 1;

  /* build the sum in place up the tree */
  for(int d = prefix_len>>1; d > 0; d >>=1) {
    barrier(CLK_LOCAL_MEM_FENCE);

    if(id < d) {
      int ai = offset*(2*id + 1) - 1;
      int bi = offset*(2*id + 2) - 1;
      prefix_buffer[bi] += prefix_buffer[ai];
    }

    offset *= 2;
  }

  /* scan back down the tree */

  /* clear the last element */
  if(id == 0) {
    prefix_buffer[prefix_len-1] = 0;
  }

  /* traverse down the tree building the scan in the place */
  for(int d = 1; d < prefix_len ; d *= 2) {
    offset >>=1;
    barrier(CLK_LOCAL_MEM_FENCE);
		
    if(id < d) {
      int ai = offset*(2*id + 1) - 1;
      int bi = offset*(2*id + 2) - 1;
			
	  unsigned int t = prefix_buffer[ai];
      prefix_buffer[ai] = prefix_buffer[bi];
      prefix_buffer[bi] += t;
    }
  }

  barrier(CLK_LOCAL_MEM_FENCE);

  //}

  // TODO: Deferred handling of long entries.

  /*
  // Code based on naive algorithm presented in:
  // Parallel Prefix Sum (Scan) with CUDA, Mark Harris, April 2007, NVIDIA.
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
  */
}

void copy_from_other_compute_unit(volatile __global unsigned int *out_queue,
                                  volatile __global unsigned int *other_in_queue) {

  unsigned qe = out_queue[1];
  //Note: This block_size is important to avoid deadlock!  In
  // the case where both queues are empty, we don't want an
  // already processed block of 256 ping ponging between the
  // cores. By making sure the first one to steal stays below 
  // the stealing threashold, we ensure that this doesn't happen.
  // Yes, this actually happened.
  unsigned block_size = 256 - queue_length(out_queue);
  unsigned block_start = (QUEUE_SIZE+other_in_queue[1]-block_size)%QUEUE_SIZE;
  //This could be SIMD, but is that worth it?
  for(int i = 0; i < 256; i++) {
    out_queue[(qe + i) % QUEUE_SIZE + 2] = other_in_queue[(block_start + i) % QUEUE_SIZE + 2];
  }
  out_queue[1] = qe + block_size;
}

// TODO: Make queues length that is a multiple of work items.

// TODO: Rename queue to frontier!

// TODO: If there is a lot of space left in old_queue, just write to that one and do not go back?

__kernel void mark_step(volatile __global unsigned int *space,
                        __local unsigned int *prefix_buffer,
                        volatile __global unsigned int *old_queue,
                        volatile __global unsigned int *new_queue,
                        unsigned int base_ptr, 
                        __local unsigned int *scratch) { /*unused*/
  // TODO: Maybe want to copy this into local space.


  //THIS BIT IS THE DUAL COMPUTE UNIT HACKERY
#if USE_BOTH_COMPUTE
  unsigned queue_index = get_global_id(0) / get_local_size(0);

  if( false && queue_index > 0 ) {
    return;
  }

  volatile __global unsigned int *in_queue = old_queue + queue_index*(QUEUE_SIZE+2);
  volatile __global unsigned int *out_queue = new_queue + queue_index*(QUEUE_SIZE+2);
#else
  volatile __global unsigned int *in_queue = old_queue;
  volatile __global unsigned int *out_queue = new_queue;
#endif

  int id = get_local_id(0);

  // Count the number of iterations - do at least MIN_GPU_ITERATIONS of them before going back
  // to the CPU (otherwise could have frequent switching back and forth - expensive!).
  int i = 0;

  //Wait until _both_ buffers are empty
  //PAR: One may exit
  //If insufficient level of parallelism, fall back to CPU.
  while( (queue_length(in_queue) > CPU_FALLBACK) ||
         (queue_length(out_queue) > CPU_FALLBACK) ||
         i < MIN_GPU_ITERATIONS) {
    //both workgroups must make the same choice
    // Note needed: If they were synced before, they still are.
    //barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

    i++;
    if(i > 200000) {
      return;
    }

    //There can be multiple work groups attempting to
    // pop from the queue at once.

    //Can't unlock until we're done reading from the location in the queue
    // TODO: Copy to local first.
    range_in_queue inblock = get_region_from_input(in_queue);
    
    int qs = inblock.begin;
    int length = inblock.length;

    // Read from memory in vectors.
    uint4 cur_vec;

    //Sort that region so that the memory accesses are nicely aligned.
    //Perf note: On the micro benchmarks tried, this took 2x the
    // time, for no benefit in memory access.  Mind you, wasn't doing
    // uniquification yet, but the sorting should have helped and really
    // didn't.
    //ParallelBitonic_Local(length, &in_queue[qs + 2],&in_queue[qs + 2],scratch);


    //Required since we may run one iteration with an empty queue (to
    // avoid global optimization check below)
    if( length <= 0 ) {
      //Need to make sure we unlock
    } else {

      unsigned int local_length = 0; // Number of references that need to be visited for this object.
      unsigned int real_addr = 0; // Offset into the heap structure array. (which was copied from the original heap structure)
      
      // Step 1: Mark the node and write number of references to be handled.
      if (id < length) {
        // Offset into the heap structure that contains this object's data.
        unsigned int addr = in_queue[((qs+id) % QUEUE_SIZE) + 2]; // Translate from address into array offset.
	
        if (addr != NULL_REF) {
          real_addr = (addr - base_ptr) >> 2;
	  
          // Header contains mark bit AND number of references. This gives us locality of access!
          //Note: Despite appearances, we don't actually want 
          // this to be an atomic update.  That forces us along
          // the complete memory path for the entire kernel
          // which is much slower.  Instead, we accept some
          // redundant work for the sake of keeping on the
          // fast memory path. 

#if USE_VECTORS
          __global uint4* vspace = ((__global uint4*)(space));
          cur_vec = vspace[(real_addr+HEADER_OFFSET)/4];
          unsigned int header = cur_vec.s0;
#else
          unsigned int header = space[real_addr+HEADER_OFFSET];
#endif
          // Try writing back only if necessary. Should reduce memory contention.
          //space[real_addr+HEADER_OFFSET] = header | (uint)MARK_MASK;
          //unsigned int header = atomic_or(&space[real_addr+HEADER_OFFSET], (uint)MARK_MASK);
	  
#if 1
          // Has it not been marked yet?
          local_length=select(local_length, (header & REFCOUNT_MASK), 
                              !(header & MARK_MASK));
          space[real_addr+HEADER_OFFSET] = header | (uint)MARK_MASK;
#else
          if (!(header & MARK_MASK)) {
            // Add all its references to the queue.
            local_length = header & REFCOUNT_MASK;
            space[real_addr+HEADER_OFFSET] = header | (uint)MARK_MASK;

          }
#endif
        }
      }

      //This is the old prefix sum implementation.
#if USE_PREFIX_SUM
      // Need to run on array of size that is a power of two.
      // TODO Can optimize for shorter arrays by using clz function.
      int prefix_len = 1 << (31 - clz(length)); // Round up to next power of 2.
      // prefix_len is strictly greater or equal to length
      if (id < prefix_len) // Last entry needs to be set to 0.
        prefix_buffer[id] = local_length; //may be zero if beyond length

      barrier(CLK_LOCAL_MEM_FENCE);
      int last_one = prefix_buffer[length-1];

      // Ensure consistent view of prefix_buffer before doing prefix sum.
      // Not needed, the prefix sum algorithm handles this
      //barrier(CLK_LOCAL_MEM_FENCE);

      // =====================================================================
      // Step 2: Prefix sum over the number of nodes (to find global offsets).
      // =====================================================================
      //looses last value
      compute_prefix_sum(prefix_buffer, prefix_len);

      // TODO: Deferred handling of long entries.

      // ======================================
      // Step 3: Copy entries to new locations.
      // ======================================

      //Is local length bounded?!

      //assert( length > 0 )
      int length_new_block = prefix_buffer[length-1] + last_one;
      range_in_queue outblock = allocate_region_in_output(out_queue, length_new_block);
      int out_qs = outblock.begin;

      if (id < length) {
        for (int j = 0; j < local_length; j++) {
          // Unroll for memory coalescing
          // TODO: Store as vectors.
          int offset = 0;
          offset = prefix_buffer[id];
          out_queue[((out_qs + offset + j) % QUEUE_SIZE)+2] = space[real_addr+REFS_OFFSET+j];
        }
      }
#else
      prefix_buffer[id] = 0;

      barrier(CLK_LOCAL_MEM_FENCE);

      int local_offsets[16];

      atomic_add(&prefix_buffer[200], local_length); // overall sum.
      #if USE_DIVERGENCE
      scratch[0] = 0; // reset buffer for large objects.
      #endif

      atomic_max(&prefix_buffer[199], local_length); // maximum local length.

      barrier(CLK_LOCAL_MEM_FENCE);

      #if USE_DIVERGENCE
      int ml = prefix_buffer[199];
      int max_length = min(DIVERGENCE_CUTOFF, ml);
      int have_divergence = (ml > DIVERGENCE_CUTOFF);
      #else
      int max_length = prefix_buffer[199];
      #endif

      int length_new_block = prefix_buffer[200];

      range_in_queue outblock = allocate_region_in_output(out_queue, length_new_block);

      int out_qs = outblock.begin;

      //if (id < length) {
        //const int offset = prefix_buffer[id];
        int offset = 0;

#if !USE_VECTORS
        for (int k = 0; k < max_length; k+=16) {
          barrier(CLK_LOCAL_MEM_FENCE);

          prefix_buffer[id] = 0;

          barrier(CLK_LOCAL_MEM_FENCE);

          for (int j = k; j < local_length && j < k+16; j++) {
            local_offsets[j%16] = atomic_inc(&prefix_buffer[j%16]);
          }
          
          barrier(CLK_LOCAL_MEM_FENCE);
            
          for (int j = k; j < local_length && j < k+16; j++) {
            out_queue[((out_qs + offset + local_offsets[j%16]) % QUEUE_SIZE)+2] = space[real_addr+REFS_OFFSET+j];
            offset += prefix_buffer[j%16];
          }
        }
#else

        // First round of the histogram.
        for (int j = 0; j < local_length && j < 16; j++) {
          local_offsets[j] = atomic_inc(&prefix_buffer[j]);
        }
          
        barrier(CLK_LOCAL_MEM_FENCE);
      
        //read three from header vector
        if (0 < local_length) {
          out_queue[((out_qs + offset + local_offsets[0]) % QUEUE_SIZE)+2] = cur_vec.s1;
          offset += prefix_buffer[0];
                
        }
        if (1 < local_length) {
          out_queue[((out_qs + offset + local_offsets[1]) % QUEUE_SIZE)+2] = cur_vec.s2;
          offset += prefix_buffer[1];
        }  
        
        if (2 < local_length) {
          out_queue[((out_qs + offset + local_offsets[2]) % QUEUE_SIZE)+2] = cur_vec.s3;
          offset += prefix_buffer[2];
        }
        
        //} 

        //loop over vectors of four
        __global uint4* vspace = ((__global uint4*)(space));
        for (int k = 3; k < max_length; k+=4) {
          // Every 16 references, recalculate the next 16 references.

          // Current implementation might lead to duplicate calculations!
          if ((k%16)+3 >= 16) {
            barrier(CLK_LOCAL_MEM_FENCE);

            prefix_buffer[id] = 0;

            barrier(CLK_LOCAL_MEM_FENCE);

            for (int j = k; j < local_length && j < k+16; j++) {
              local_offsets[j%16] = atomic_inc(&prefix_buffer[j%16]);
            }
          
            barrier(CLK_LOCAL_MEM_FENCE);
          }

          if (k < local_length)
            cur_vec = vspace[(real_addr+REFS_OFFSET+k)/4];

          if (k < local_length) {
            out_queue[((out_qs + offset + local_offsets[k%16]) % QUEUE_SIZE)+2] = cur_vec.s0;
            offset += prefix_buffer[k%16];
          }

          if (k+1 < local_length) {
            out_queue[((out_qs + offset + local_offsets[(k+1)%16]) % QUEUE_SIZE)+2] = cur_vec.s1;
            offset += prefix_buffer[(k+1)%16];
          }

          if (k+2 < local_length) {
            out_queue[((out_qs + offset + local_offsets[(k+2)%16]) % QUEUE_SIZE)+2] = cur_vec.s2;
            offset += prefix_buffer[(k+2)%16];
          }

          if (k+3 < local_length) {
            out_queue[((out_qs + offset + local_offsets[(k+3)%16]) % QUEUE_SIZE)+2] = cur_vec.s3;
            offset += prefix_buffer[(k+3)%16];
          }


          //TODO: Check alignments in Jikes code
          //vector path
          //__global uint4* vspace = ((__global uint4*)(space));
          //__global uint4* vqueue = ((__global uint4*)(out_queue));
          //uint4 refvec = vspace[(real_addr+REFS_OFFSET+k)/4];
          //unsigned int queue_index = ((out_qs + offset + k) % QUEUE_SIZE)+2;
          //vqueue[ queue_index / 4 ] = refvec; 
        }
      //}
#endif
#endif

      #if USE_DIVERGENCE

      // Only handle divergence if necessary.
      if (have_divergence) {

      barrier(CLK_LOCAL_MEM_FENCE);

      // a large object? put into buffer.
      if (local_length > DIVERGENCE_CUTOFF) {
        int div_offset = atomic_inc(&scratch[0]);
        scratch[div_offset+1] = real_addr;
      }

      barrier(CLK_LOCAL_MEM_FENCE);

      int num_big_objs = scratch[0];

      for (int i = 0; i < num_big_objs; i++) {
        int addr = scratch[i+1];
        unsigned int header = space[addr+HEADER_OFFSET];
        int refcount = header & REFCOUNT_MASK;

        range_in_queue outblock = allocate_region_in_output(out_queue, refcount-DIVERGENCE_CUTOFF);
        int out_qs = outblock.begin;

        for (int j = DIVERGENCE_CUTOFF+id; j < refcount; j += 256) {
          out_queue[((out_qs+j-DIVERGENCE_CUTOFF) % QUEUE_SIZE) + 2] = space[addr+REFS_OFFSET+j];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
      }

      }
      #endif
    }
   
    // Not needed, switching your local view of the buffers is fine as long as you can't
    // actually operate on them.  Only the last sync neeeded
    //barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

    //Optimization note: It's better to _not_ check if the queue we're
    // switching to is empty.  The common case is that it isn't and 
    // global access is expensive!
    
    //switch over to the other buffer - Note:
    // all work groups _must_ switch together!  We
    // reading from data that's still in the queue
    // even though we've updated the start index!
    volatile __global unsigned int *tmp_queue = in_queue;
    in_queue = out_queue;
    out_queue = tmp_queue;
      
    barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);


#if USE_BOTH_COMPUTE
#if USE_LOAD_BALENCE
    //should we try to grab from the other queue?
    if( (queue_length(in_queue) < 256) &&
        (queue_length(out_queue) < 256) ) {

      //get the other one
      unsigned queue_index = get_global_id(0) / 256;
      queue_index = 1-queue_index;

      volatile __global unsigned int *other_in_queue = old_queue + queue_index*(QUEUE_SIZE+2);
      volatile __global unsigned int *other_out_queue = new_queue + queue_index*(QUEUE_SIZE+2);

      //Note: The threashold here is important, don't change it without
      // reading the note in the called function.
      if( queue_length(other_out_queue) > 256 ) {
        copy_from_other_compute_unit(in_queue,
                                     other_out_queue);
      }
      if( queue_length(other_in_queue) > 256 ) {
        copy_from_other_compute_unit(in_queue,
                                     other_in_queue);
      }
    } //end copy over
    barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

#endif //#if USE_LOAD_BALENCE
#endif //#if USE_BOTH_COMPUTE
  }

}
