/** @file This header defines things used among
    the various files in this directory.  It is
    not public API. */

#ifndef GPUGC_INTERNAL_H
#define GPUGC_INTERNAL_H


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

#include <CL/cl.h>
#include <CL/cl_ext.h>

#include "common.h"

int init_opencl(void);
int shutdown_opencl(void);
int launch_opencl_mark(void *head, void *tail, void *start, void *end);

struct refgraph_entry {
  void *object;
  uint32_t num_edges;
  //Number of elements == num_edges
  struct refgraph_entry *edges[1];
};

struct frontier_queue {
  uint32_t qs;
  uint32_t qe;
  struct refgraph_entry *q[QUEUE_SIZE];
};



/** This function dumps the figures for the paper based on
    analysis of the reachable portion of the reference graph.
    
    Note: This operates on a _assumed to be valid_
    reference graph and the queue.  It is the callers
    responsability to perform sanity checking!
    
    Note: This function has the side effect of doing a mark
    and emptying the proviced queue!

    (See analyze.cxx for implementation.)
*/
void analyze_heap_structure(struct refgraph_entry** q,
                            uint32_t& qs,
                            uint32_t& qe);


typedef struct stat_entry_t {
  uint32_t mem_size; //size of the space in bytes
  float cpu_time; //cpu mark time (in algorithm)
  float gpu_executionTimeInMilliseconds; //(gpu execute)
  float gpu_submitTimeInMilliseconds;
  float gpu_queueTimeInMilliseconds;
  float check_time; //time to check mark (on cpu)
  float overall;
  stat_entry_t();
} stat_entry ;


struct stat_manager_t {
  std::ofstream stream;
  float startup;
  float shutdown;
  std::vector<stat_entry> stats;

  void dump_to_file();
};

extern struct stat_manager_t g_stat_manager;

void validate_structure(void* p_roots_begin, void* p_roots_end, void* p_space_begin, void* p_space_end);

void set_use_local_paths();


//extract to header

//#define DEBUG
#ifdef DEBUG
#define DEBUGF printf
#else
#define DEBUGF //printf
#endif

#endif //GPUGC_INTERNAL_H
