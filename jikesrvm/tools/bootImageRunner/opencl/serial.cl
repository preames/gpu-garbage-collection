
/*
  NOTE TO THE READER: The contents of the common.h header file are
  prepended here when the code is loaded.
 */

inline unsigned queue_length(volatile __global unsigned int* q) {
  const unsigned qs = q[0];
  const unsigned qe = q[1];
  //handle the wrap around case
  return ((qe + QUEUE_SIZE - qs) % QUEUE_SIZE);
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
  unsigned int work_group_size = 1;

  unsigned int qs = queue[0];
  unsigned int qe = queue[1];
  //ex: 2,3,100->1, 99,1,100->2
  unsigned int length = min(work_group_size,  queue_length(queue) );
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

// TODO: Make queues length that is a multiple of work items.

// TODO: Rename queue to frontier!

// TODO: If there is a lot of space left in old_queue, just write to that one and do not go back?

__kernel void mark_step(volatile __global unsigned int *space,
                        __local unsigned int *prefix_buffer,
                        volatile __global unsigned int *old_queue,
                        volatile __global unsigned int *new_queue,
                        unsigned int base_ptr, 
                        __local unsigned int *scratch) {
  // TODO: Maybe want to copy this into local space.
  volatile __global unsigned int *in_queue = old_queue;

  int id = get_local_id(0);
  if( get_local_size(0) > 1 ) {
    return;
  }

  // Count the number of iterations - do at least MIN_GPU_ITERATIONS of them before going back
  // to the CPU (otherwise could have frequent switching back and forth - expensive!).

  //Wait until _both_ buffers are empty
  //PAR: One may exit
  //If insufficient level of parallelism, fall back to CPU.
  while( queue_length(in_queue) > 0 ) { 
    //Can't unlock until we're done reading from the location in the queue
    // TODO: Copy to local first.
    range_in_queue inblock = get_region_from_input(in_queue);
    
    int qs = inblock.begin;
    int length = inblock.length;

    //Required since we may run one iteration with an empty queue (to
    // avoid global optimization check below)
    unsigned int local_length = 0; // Number of references that need to be visited for this object.
    unsigned int real_addr = 0; // Offset into the heap structure array. (which was copied from the original heap structure)
      
    // Step 1: Mark the node and write number of references to be handled.
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

      unsigned int header = space[real_addr+HEADER_OFFSET];
	  
      // Has it not been marked yet?
      local_length=select(local_length, (header & REFCOUNT_MASK), 
                          !(header & MARK_MASK));
      space[real_addr+HEADER_OFFSET] = header | (uint)MARK_MASK;
    }
    
    range_in_queue outblock = allocate_region_in_output(in_queue, local_length);

    int out_qs = outblock.begin;

    for (int j = 0; j < local_length; j++) {
      in_queue[((out_qs + j) % QUEUE_SIZE)+2] = space[real_addr+REFS_OFFSET+j];
    }
    
  } //end while
} //end kernel
