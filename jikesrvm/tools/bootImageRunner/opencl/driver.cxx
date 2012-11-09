
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
//public interface
#include "gpugc.h"
//internal functions and types
#include "gpugc-internal.h"

#include "common.h"
#include "gpugc-utils.h"
#include "utils.h"


uint32_t dummy_object;

/// Generate a binary tree in the reference graph format we're using
uint32_t* test_generate_tree(const int n, uint32_t *space, unsigned int *offset) {

	if (n == 0)
	  return NULL;

	uint32_t* ptr = space+(*offset);
	space[(*offset)++] = (uint32_t)&dummy_object; //dummy object
    space[(*offset)++] = 8; //flag bits & ref count

	int ref_start = *offset;
	(*offset) += (8+2); // 2 words padding.

	space[ref_start] = (uint32_t)test_generate_tree(n-1, space, offset); 
	space[ref_start+1] = (uint32_t)test_generate_tree(n-1, space, offset);
	space[ref_start+2] = (uint32_t)test_generate_tree(n-1, space, offset);
	space[ref_start+3] = (uint32_t)test_generate_tree(n-1, space, offset);
	space[ref_start+4] = (uint32_t)test_generate_tree(n-1, space, offset);
	space[ref_start+5] = (uint32_t)test_generate_tree(n-1, space, offset);
	space[ref_start+6] = (uint32_t)test_generate_tree(n-1, space, offset);
	space[ref_start+7] = (uint32_t)test_generate_tree(n-1, space, offset);

	return ptr;
}

uint32_t* test_generate_wide(const int n, uint32_t *space, unsigned int *offset) {
	space[(*offset)++] = (uint32_t)&dummy_object; //dummy object
    space[(*offset)++] = n; //flag bits & ref count

	int ref_end = (*offset) + n;

	for (int i = 0; i < n; i++) {
      space[(*offset)++] = (uint32_t)&space[ref_end]; //flag bits & ref count
      space[ref_end++] = (uint32_t)&dummy_object;
      space[ref_end++] = 0;
      ref_end += 2;
	}

	*offset = ref_end;

    return space+(*offset);
}

// Check that everything was marked correctly.
int check_mark(uint32_t *space, int offset) {
  if (offset == 0x3fffffff)
	return 1;

  if (!(space[offset+1] & 0x80000000)) {
    DEBUGF("Unmarked node: %d\n", offset);
    return 0;
  }

  check_mark(space, space[offset+2] >> 2);
  check_mark(space, space[offset+4] >> 2);

  return 1;
}



uint32_t* test_generate_single(uint32_t* space, uint32_t size_in_bytes) {
  assert( size_in_bytes > 8 );
  ((struct refgraph_entry*)space)->object = (void*)&dummy_object;
  ((struct refgraph_entry*)space)->num_edges = 0; //clear bitfield (and no real edges)
  return space+4;
}

uint32_t* test_generate_linked_list(struct refgraph_entry* space, uint32_t size_in_bytes, int length=10) {
  assert( size_in_bytes > 16*length+8 );
  for(int i = 0; i < length-1; i++) {
    space->object = (void*)&dummy_object;
    space->num_edges = 1; //clear bitfield and set num_edges
    space->edges[0] = (struct refgraph_entry*)((uint32_t*)space+4); // consider padding.
    space = (struct refgraph_entry*)((uint32_t*)space+4); // 1 word padding.
  }
  space->object = (void*)&dummy_object;
  space->num_edges = 0; //clear bitfield (and no real edges)
  return ((uint32_t*)space+4); // 1 word padding.
}
void bestcase() {
  uint32_t bufsizeinwords = 1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  uint32_t *space = mem + 3; // offset for alignment.
	  
  //a single sample root to that data
  uint32_t roots[256];
  int roots_length = 256;

  //Store in inverse order to avoid stride 1
  uint32_t* place = space;
  for(int i = 255; i >= 0; i--) {
    roots[i] = (uint32_t)(place);
    place = test_generate_linked_list((struct refgraph_entry*)(place), 
                                      (space+bufsizeinwords-place)*sizeof(uint32_t));
  }
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)((char*)space+bufsizeinbytes-12));
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)place);

  free( mem );
  mem = NULL;
  space = NULL;
}

void selftest() {
  uint32_t bufsizeinwords = 1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  // For alignment, need an offset of 3 elements at the beginning.
  // So the first element that is vector-aligned is the header.
  uint32_t *space = mem + 3;
	  
  //fill in that space (with test data)
  test_generate_single(space, bufsizeinbytes );
	  
  //a single sample root to that data
  uint32_t roots[5];
  roots[0] = (uint32_t)space; //address of our test data's first node
  int roots_length = 1;

  //Make sure the data structures passed in are correct
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)((char*)space+bufsizeinbytes));
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  memset(mem, bufsizeinbytes, 0);
  test_generate_linked_list((struct refgraph_entry*) space, bufsizeinbytes);
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)((char*)space+bufsizeinbytes-12));
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  memset(mem, bufsizeinbytes, 0);
  unsigned offset = 0;
  test_generate_tree(4, space, &offset);
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)((char*)space+bufsizeinbytes-12));
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  memset(mem, bufsizeinbytes, 0);
  offset = 0;
  test_generate_tree(5, space, &offset);
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)((char*)space+bufsizeinbytes-12));
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  memset(mem, bufsizeinbytes, 0);
  offset = 0;
  test_generate_tree(6, space, &offset);
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)((char*)space+bufsizeinbytes-12));
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  memset(mem, bufsizeinbytes, 0);
  offset = 0;
  test_generate_wide(300, space, &offset);
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)((char*)space+bufsizeinbytes-12));
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  free( mem );
  mem = NULL;
  space = NULL;
}

void pretty_print_size(std::ostream& stream, uint64_t size) {
  if(0 != size % 1024 ) {
    stream << size << " bytes";
    return;
  }
  size /= 1024;
  if(0 != size % 1024 ) {
    stream << size << " KB";
    return;
  }
  size /= 1024;
  stream << size << " MB";
}

void microbench_prime() {
  uint32_t bufsizeinwords = 63*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  // For alignment, need an offset of 3 elements at the beginning.
  // So the first element that is vector-aligned is the header.
  uint32_t *space = mem + 3;
	  
	  
  //a single sample root to that data
  uint32_t roots[5];
  roots[0] = (uint32_t)space; //address of our test data's first node
  int roots_length = 1;

  //prime the system
  for(int i = 0; i < 5; i++) {
    printf("Priming system.  Ignore.\n");
    //fill in that space (with test data)
    //need to clear the mark bits each iteration
    uint32_t* end = test_generate_single(space, bufsizeinbytes );

    //Make sure the data structures passed in are correct
    validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                        (void*)space, (void*)end);
    launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                                (void*)mem, (void*)end);

  }
  free( mem );
  mem = NULL;
  space = NULL;
}


/** Show the increase in kernel launch time */
void microbench_launch() {
  uint32_t bufsizeinwords = 63*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  // For alignment, need an offset of 3 elements at the beginning.
  // So the first element that is vector-aligned is the header.
  uint32_t *space = mem + 3;
	  
	  
  //a single sample root to that data
  uint32_t roots[5];
  roots[0] = (uint32_t)space; //address of our test data's first node
  int roots_length = 1;

  //prime the system
  for(int i = 0; i < 5; i++) {
    printf("Priming system.  Ignore.\n");
    //fill in that space (with test data)
    //need to clear the mark bits each iteration
    test_generate_single(space, bufsizeinbytes );

    //Make sure the data structures passed in are correct
    validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                        (void*)space, (void*)((char*)space+bufsizeinbytes));
    launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                                (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  }

  std::ofstream stream("micro-launch-size.dat");
  stream << 
    "# THIS FILE IS MACHINE GENERATED.\n"
    "# Format:\n"
    "# column 1 - size of memory in bytes\n"
    "# column 2 - average overhead\n"
    "# column 3 - minimum overhead\n"
    "# column 4 - max overhead\n";

  //try power of two larger memory buffers to see what effect this has on the 
  // launch overhead
  for(uint64_t size = 64; size <= bufsizeinbytes; size*=2) {
    std::cout << "Testing space size ";
    pretty_print_size(std::cout, size);
    std::cout << std::endl;
    std::vector<float> overheads;
    for(int i = 0; i < 20; i++) {
      //fill in that space (with test data)
      //need to clear the mark bits each iteration
      test_generate_single(space, bufsizeinbytes );
      
      //Make sure the data structures passed in are correct
      validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                          (void*)space, (void*)((char*)space+size));
      launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                                  (void*)mem, (void*)((char*)mem+size));
      float overhead = g_stat_manager.stats.back().gpu_queueTimeInMilliseconds - g_stat_manager.stats.back().gpu_executionTimeInMilliseconds;

      overheads.push_back( overhead );
    }
    float low = min(overheads);
    float high = max(overheads);
    float s = sum(overheads);
    stream << " " << size << " " << s / 20 << " " << low << " " << high << std::endl;
  }

  free( mem );
  mem = NULL;
  space = NULL;
}


void repeattest(int iterations) {
	DEBUGF("Running %d iterations of the GC test loop.\n", iterations);
	
	for(int i = 0; i < iterations; i++) {
	  if( iterations != 1 ) {
	    DEBUGF("\n");
	    DEBUGF("----------- Iteration %d --------\n", i);
	  }

	  // Allocate space for heap structure.
	  const int bufSize = 1024*1024; //in words
	
	  DEBUGF("Allocating buffer...\n");
	  uint32_t *space = (uint32_t*)malloc(bufSize*sizeof(uint32_t));
      assert( space );
	  memset(space, bufSize*sizeof(uint32_t), 0);
	  DEBUGF("Test ptr: %p\n", space);
	  //fill in that space (with test data)
	  unsigned offset = 0;
	  test_generate_tree(4, space, &offset);

	  // Generate test data.
	  uint32_t roots[5];
	  roots[0] = (uint32_t)space; //address of our test data's first node
	  int roots_length = 1;

	  DEBUGF("Tree elements: %d (offset: %d)\n", offset);

	  DEBUGF("%d, %d", sizeof(uint32_t), sizeof(uint32_t*));
	  DEBUGF("%p, %p", &roots[0], &roots[0]+roots_length);
	  launch_checked_opencl_mark(&roots[0], &roots[0]+roots_length, space, space+bufSize);

	  free(space);
	  space = NULL;
	}
}

void mb_single_item() {
  uint32_t bufsizeinwords = 1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);

  // For alignment, need an offset of 3 elements at the beginning.
  // So the first element that is vector-aligned is the header.
  uint32_t *space = mem + 3;
	  
  //a single sample root to that data
  uint32_t roots[5];
  roots[0] = (uint32_t)space; //address of our test data's first node
  int roots_length = 1;

  memset(mem, bufsizeinbytes, 0);
  uint32_t* end = test_generate_single(space, bufsizeinbytes );

  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)end);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)end);

  free( mem );
  mem = NULL;
  space = NULL;
}


void mb_single_10kll() {
  uint32_t bufsizeinwords = 1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);

  // For alignment, need an offset of 3 elements at the beginning.
  // So the first element that is vector-aligned is the header.
  uint32_t *space = mem + 3;
	  
  //a single sample root to that data
  uint32_t roots[5];
  roots[0] = (uint32_t)space; //address of our test data's first node
  int roots_length = 1;

  memset(mem, bufsizeinbytes, 0);
  uint32_t* end = test_generate_linked_list((struct refgraph_entry*) space, bufsizeinbytes, 10000);
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)end);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)end);

  free( mem );
  mem = NULL;
  space = NULL;
}

void mb_256_by_10kll() {
  uint32_t bufsizeinwords = 30*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  uint32_t *space = mem + 3; // offset for alignment.
	  
  //a single sample root to that data
  uint32_t roots[256];
  int roots_length = 256;

  //Store in inverse order to avoid stride 1
  uint32_t* place = space;
  for(int i = 255; i >= 0; i--) {
    roots[i] = (uint32_t)(place);
    place = test_generate_linked_list((struct refgraph_entry*)(place), 
                                      (space+bufsizeinwords-place)*sizeof(uint32_t),
                                      10000);
  }

  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)place);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)place);

  free( mem );
  mem = NULL;
  space = NULL;
}


void mb_2560_by_ll() {
  uint32_t bufsizeinwords = 40*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  uint32_t *space = mem + 3; // offset for alignment.
	  
  //a single sample root to that data
  uint32_t roots[2560];
  int roots_length = 2560;

  //Store in inverse order to avoid stride 1
  uint32_t* place = space;
  for(int i = 2560-1; i >= 0; i--) {
    roots[i] = (uint32_t)(place);
    place = test_generate_linked_list((struct refgraph_entry*)(place), 
                                      (space+bufsizeinwords-place)*sizeof(uint32_t),
                                      3000);
  }
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)place);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)place);

  free( mem );
  mem = NULL;
  space = NULL;
}

void mb_unbalenced() {
  uint32_t bufsizeinwords = 30*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  uint32_t *space = mem + 3; // offset for alignment.
	  
  //a single sample root to that data
  uint32_t roots[512];
  int roots_length = 512;

  //Store in inverse order to avoid stride 1
  uint32_t* place = space;
  for(int i = 255; i >= 0; i--) {
    roots[i] = (uint32_t)(place);
    place = test_generate_linked_list((struct refgraph_entry*)(place), 
                                      (space+bufsizeinwords-place)*sizeof(uint32_t),
                                      1000);
  }
  uint32_t offset = 0;
  for(int i = 255; i >= 0; i--) {
    roots[i+256] = (uint32_t)test_generate_tree(4, place, &offset);
  }
  uint32_t* end = place + offset;
  assert( end < space + bufsizeinwords );

  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)end);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)end);

  free( mem );
  mem = NULL;
  space = NULL;
}

void mb_divergence() {
  uint32_t bufsizeinwords = 30*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  uint32_t *space = mem + 3; // offset for alignment.
	  
  //a single sample root to that data
  uint32_t roots[1];
  int roots_length = 1;

  unsigned int offset = 0;
  uint32_t* end = test_generate_wide(1000, space, &offset);
  roots[0] = (uint32_t)(space);
  
  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)end);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)end);

  free( mem );
  mem = NULL;
  space = NULL;
}



void mb_vp_ll() {
  uint32_t bufsizeinwords = 30*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  uint32_t *space = mem + 3; // offset for alignment.
	  
  //a single sample root to that data
  uint32_t roots[10];
  int roots_length = 1;

  /* From VP pg 7.
     Concurrently 16 Cuda kernels allocate lists with 8192 elements
     each. One list is kept, the other 15 lists become collectable. 
     This is repeated 8 times.
     
     From inspection of the code, the 8 times are separate gcs.  We perform each
     micro benchmark multiple times, so we skip this.
  */

  //Store in inverse order to avoid stride 1
  uint32_t* place = space;
  //generate 1 sets of linked lists
  for(int i = 1; i >= 0; i--) {
    roots[i] = (uint32_t)(place);
    //generate 16, keep a pointer to only one
    for(int j = 0; j < 16; j++) {
      place = test_generate_linked_list((struct refgraph_entry*)(place), 
                                        (space+bufsizeinwords-place)*sizeof(uint32_t),
                                        8192);
    }
  }
  assert( place < space + bufsizeinwords );

  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)place);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)place);

  free( mem );
  mem = NULL;
  space = NULL;
}


void mb_vp_array() {
  uint32_t bufsizeinwords = 30*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  DEBUGF("Self ptr: %p\n", mem);
  memset(mem, bufsizeinbytes, 0);

  uint32_t *space = mem + 3; // offset for alignment.
	  
  //a single sample root to that data
  uint32_t roots[512];
  int roots_length = 64;

  /* From VP page 7, col 2:
     The bottom chart of Fig. 9 shows the results of repeatedly allo-
     cating large arrays of references to fresh objects. This benchmark
     has 1024 parallel kernels each of which creates arrays filled with
     1024 small objects. The arrays of only 64 kernels are kept. The
     other 960 arrays (and pointed to objects) become garbage.
  */

  
  unsigned int offset = 0;
  for(int i = 0; i < 1024; i++) {
    assert( space + offset < space + bufsizeinwords );
    if( i < 64 ) {
      roots[i] = (uint32_t)(&space[0]+offset);
    }
    test_generate_wide(1024, space, &offset);
  }
  uint32_t* end = space + offset;
  assert( end < space + bufsizeinwords );


  validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)space, (void*)end);
  launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), (void*)mem, (void*)end);

  free( mem );
  mem = NULL;
  space = NULL;
}


void run_bm(const char* fname, void (*bench_fun)()) {
  printf(fname);
  printf("\n");
  g_stat_manager.stream.open(fname);

  for(int i = 0; i < 20; i++) {
    bench_fun();
  }

  g_stat_manager.dump_to_file();
  g_stat_manager.stats.clear();
}  


//
void microbenchs_for_eval() {
  printf("Running micro benchmarks\n");

  //clear anything done so far
  g_stat_manager.stream.close();
  g_stat_manager.stats.clear();

  //run_bm("microbench_single.dat", mb_single_item );
  //run_bm("microbench_single_10kll.dat", mb_single_10kll );
  //run_bm("microbench_256_by_10kll.dat", mb_256_by_10kll );
  run_bm("microbench_2560_by_100ll.dat", mb_2560_by_ll );
  run_bm("microbench_unbalenced.dat", mb_unbalenced );
  //run_bm("microbench_divergence.dat", mb_divergence );
  /* These couple are aproximations of the VP paper */
  //run_bm("microbench_vp_ll.dat", mb_vp_ll );
  //run_bm("microbench_vp_array.dat", mb_vp_array );

  //force a flush
  g_stat_manager.stream.open("stat.dat");




}





int main(int argc, char **argv) {
  //must be called before anything else!
  set_use_local_paths();


  //before anything else, run some sanity checks
  DEBUGF("Running self test sequence\n");
  selftest();

  //none of this stuff is useful right now
  int iterations = 1;
  if( argc >= 3 && strcmp(argv[1],"--iterations")==0) {
    iterations = atoi(argv[2]);
  }
  //  repeattest(iterations);

  // WARNING: Make sure any micro benchmark you run
  // is _NOT_ the first kernel run.  Run it multiple 
  // times and ignore the first few executions.  
  //for(int i = 0; i < 20; i++) {
  //  bestcase();
  //}

  //prime the system
  microbench_prime();

  microbenchs_for_eval();




  //Did you remember to change the QUEUE_SIZE to 100?
  //printf("Running kernel launch micro benchmark\n");
  //microbench_launch();
}
