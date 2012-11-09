
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

#include <CL/cl.h>
#include <CL/cl_ext.h>

#include "common.h"
#include "gpugc-utils.h"
#include "utils.h"


//TODO: Need to plumb GPU only flag all the way through and convert current to auto

bool get_boolean_environment_variable(const char* name, bool def = false) {
  std::string val = get_environment_variable(name, def ? "true" : "false");
  if( val == "true" ) {
    return true;
  } else if ( val == "false" ) {
    return false;
  } else {
    printf("Illegal value for '%s' environment variable\n", name);
    assert(false);
  }
}



bool get_serial_flag() {
  return get_boolean_environment_variable("gpugc_serial", false);
}


bool b_use_local_paths = false;
void set_use_local_paths() {
  b_use_local_paths = true;
}
const char* get_opencl_kernel_filename() {
  if( b_use_local_paths ) {
    return get_serial_flag() ? "serial.cl" : "gpugc.cl";
  } else {
    return get_serial_flag() ? "./tools/bootImageRunner/opencl/serial.cl"
      :"./tools/bootImageRunner/opencl/gpugc.cl"; 
  }
}
const char* get_opencl_common_filename() {
  if( b_use_local_paths ) {
    return "common.h";
  } else {
    return "./tools/bootImageRunner/opencl/common.h";
  }
}

// AMD specific
cl_platform_id platform_id;
cl_uint num_of_platforms=0;
cl_uint num_of_devices=0;

cl_device_id device_id;             // compute device id 
cl_context context;                 // compute context
cl_command_queue commands;          // compute command queue
cl_program program;                 // compute program
cl_kernel kernel;                   // compute kernel

size_t native_compute_units;        //How many instruction streams does the device support?
size_t native_work_group_size;      //What is the size of one SPMD block?

#if 0
#define DEBUGF printf
#else
#define DEBUGF //printf
#endif
#define PROFILE
//#define PROFILE_GPU_ONLY //graphics
//#define PROFILE_CPU_ONLY //traditional


bool get_notrace_flag() {
  return get_boolean_environment_variable("gpugc_notrace_output", false);
}


#define TRACEF if( !get_notrace_flag() ) printf 

int64_t timespecDiff(struct timespec *timeA_p, struct timespec *timeB_p)
{
  return ((timeA_p->tv_sec * 1000000000) + timeA_p->tv_nsec) -
    ((timeB_p->tv_sec * 1000000000) + timeB_p->tv_nsec);
}

enum ExecutionMode {
  e_CPUOnly,
  e_GPUOnly,
  e_Auto, //run whichever should be run
  e_Analysis //analyze the heap graph (on cpu)
};

ExecutionMode get_execution_mode() {
  std::string val = get_environment_variable("gpugc_mode", "gpu_only");
  if( val == "cpu_only" ) {
    return e_CPUOnly;
  } else if ( val == "gpu_only" ) {
    return e_GPUOnly;
  } else if ( val == "auto" ) {
    return e_Auto;
  } else if ( val == "analysis" || val == "analyze" ) {
    return e_Analysis;
  } else {
    assert(false && "Illegal value for 'gpugc_mode' environment variable\n");
  }
}

/// Should the results of the mark be checked against a trivial cpu mark?
/// using the VISITED2 flag...
bool get_checked_flag() {
  return get_boolean_environment_variable("gpugc_checked", false);
}

/// At what level of parallism does the CPU fall back to the GPU?
int get_cpu_fallback() {
  std::string val = get_environment_variable("gpugc_cpu_fallback", "0");
  return atoi(val.c_str());
}

bool get_use_prefix_sum() {
  return get_boolean_environment_variable("gpugc_use_prefix_sum", false);
}
bool get_use_divergence() {
  return get_boolean_environment_variable("gpugc_use_divergence", false);
}
bool get_use_vectors() {
  return get_boolean_environment_variable("gpugc_use_vectors", false);
}
bool get_use_both_compute() {
  return get_boolean_environment_variable("gpugc_use_both_compute", false);
}
bool get_use_load_balence() {
  return get_boolean_environment_variable("gpugc_use_load_balence", false);
}


bool get_validate_input_flag() {
  return get_boolean_environment_variable("gpugc_validate_input", false);
}
bool get_validate_output_flag() {
  return get_boolean_environment_variable("gpugc_validate_output", false);
}

stat_entry_t::stat_entry_t()
: cpu_time(0),
  gpu_executionTimeInMilliseconds(0),
  gpu_submitTimeInMilliseconds(0),
  gpu_queueTimeInMilliseconds(0),
  check_time(0),
  overall(0),
  mem_size(0) {}

stat_entry_t element_wise_sum(const stat_entry_t& sum, const stat_entry_t& rhs) {
  stat_entry_t temp = sum;
#define quickmacro(field)  temp.field += rhs.field
  quickmacro(mem_size);
  quickmacro(cpu_time);
  quickmacro(gpu_executionTimeInMilliseconds);
  quickmacro(gpu_submitTimeInMilliseconds);
  quickmacro(gpu_queueTimeInMilliseconds);
  quickmacro(check_time);
  quickmacro(overall);
#undef quickmacro
  return temp;
}
stat_entry_t element_wise_min(const stat_entry_t& lhs, const stat_entry_t& rhs) {
  stat_entry_t temp = lhs;
#define quickmacro(field)  temp.field = (temp.field < rhs.field ) ? temp.field : rhs.field
  quickmacro(mem_size);
  quickmacro(cpu_time);
  quickmacro(gpu_executionTimeInMilliseconds);
  quickmacro(gpu_submitTimeInMilliseconds);
  quickmacro(gpu_queueTimeInMilliseconds);
  quickmacro(check_time);
  quickmacro(overall);
#undef quickmacro
  return temp;
}
stat_entry_t element_wise_max(const stat_entry_t& lhs, const stat_entry_t& rhs) {
  stat_entry_t temp = lhs;
#define quickmacro(field)  temp.field = (temp.field > rhs.field ) ? temp.field : rhs.field
  quickmacro(mem_size);
  quickmacro(cpu_time);
  quickmacro(gpu_executionTimeInMilliseconds);
  quickmacro(gpu_submitTimeInMilliseconds);
  quickmacro(gpu_queueTimeInMilliseconds);
  quickmacro(check_time);
  quickmacro(overall);
#undef quickmacro
  return temp;
}
stat_entry_t element_wise_div(const stat_entry_t& lhs, const size_t size) {
  stat_entry_t temp = lhs;
#define quickmacro(field)  temp.field = temp.field/size
  quickmacro(mem_size);
  quickmacro(cpu_time);
  quickmacro(gpu_executionTimeInMilliseconds);
  quickmacro(gpu_submitTimeInMilliseconds);
  quickmacro(gpu_queueTimeInMilliseconds);
  quickmacro(check_time);
  quickmacro(overall);
#undef quickmacro
  return temp;
}

void print_stat(std::ofstream& stream, const stat_entry_t stat, const char* label) {
  using namespace std;
  stream << label << " "
         << stat.mem_size << " "
         << stat.cpu_time << " " 
         << stat.gpu_executionTimeInMilliseconds << " " 
         << stat.gpu_submitTimeInMilliseconds << " " 
         << stat.gpu_queueTimeInMilliseconds << " " 
         << stat.check_time << " " 
         << stat.overall << endl;
}

void stat_manager_t::dump_to_file() {
  assert( stream.is_open() );

  using namespace std;
  stream << "Startup:" << startup << endl;
  stream << "Shutdown:" << shutdown << endl;

  if( stats.size() == 0 ) return;

  stat_entry sum, min, max;
  stream << "itr heap_size cpu_time gpu_executionTimeInMilliseconds gpu_submitTimeInMilliseconds gpu_queueTimeInMilliseconds check_time overall" << endl;
  for(int i = 0; i < stats.size(); i++) {
    stream << i << " " 
           << stats[i].mem_size << " " 
           << stats[i].cpu_time << " " 
           << stats[i].gpu_executionTimeInMilliseconds << " " 
           << stats[i].gpu_submitTimeInMilliseconds << " " 
           << stats[i].gpu_queueTimeInMilliseconds << " " 
           << stats[i].check_time << " " 
           << stats[i].overall << endl;

    if( i == 0 ) {
      sum = stats[0];
      min = stats[0];
      max = stats[0];
    } else {
      sum = element_wise_sum(sum, stats[i]);
      min = element_wise_min(min, stats[i]);
      max = element_wise_max(max, stats[i]);
    }
  }
  print_stat(stream, sum, "sum");
  print_stat(stream, min, "min");
  print_stat(stream, max, "max");
  stat_entry_t avg = element_wise_div(sum, stats.size());
  print_stat(stream, avg, "avg");

  stream << flush;
  stream.close();
}

  
struct stat_manager_t g_stat_manager;


/* TODO LIST:
   - replace length arguments with separate kernel arg to avoid copy
   - Clear mark bits before second call to collection
   - Look into weak pointers and why they break us
   - Hookup second gpu pipeline
   - Establish cutover point (using benchmark data) and insert (optional) break.
   - Add checks to OpenCL for long tail cases
*/

/// A utility function to confirm a region of memory is readable and writable
/// This may cause a crash if not.
void assert_read_write(volatile void* vbegin, volatile void* vend) {
  assert( vbegin <= vend );
  
  volatile char* begin = (volatile char*)vbegin;
  volatile char* end = (volatile char*)vend;

  while(begin != end) {
    *begin = *begin;
    begin++;
  }
}

int shutdown_opencl(void);

struct opencl_cleanup_t {
  bool cleanup_needed;
  opencl_cleanup_t() {
    cleanup_needed = false;
  }
  ~opencl_cleanup_t() {
    g_stat_manager.dump_to_file();
    //After the change to dynamic loading, this started hanging randomly
    // rather than fix the issue, just avoid shutdown for the benchmarking.
    if( false && cleanup_needed ) {
      shutdown_opencl();
      cleanup_needed = false;
    }
  }
} g_cleanup;

//used accross calls to GC
//please don't access this directly!
uint32_t *g_root_data = NULL;
uint32_t *g_output_queue = NULL;

size_t get_queue_alloc_size_in_bytes() {
  if( get_use_both_compute() ) {
    return 2*(QUEUE_SIZE+2)*sizeof(uint32_t);
  } else {
    return (QUEUE_SIZE+2)*sizeof(uint32_t);
  }
}

uint32_t *get_root_data_memory() {
  if( !g_root_data ) {
    g_root_data = (uint32_t*)malloc(get_queue_alloc_size_in_bytes());
    assert( g_root_data );
    DEBUGF("Root: %p\n", g_root_data);
  }
  return g_root_data;
}

uint32_t *get_output_queue_memory() {
  if( !g_output_queue ) {
    g_output_queue = (uint32_t*)malloc(get_queue_alloc_size_in_bytes());
    assert( g_output_queue );
    DEBUGF("Queue2: %p\n", g_output_queue);
  }
  return g_output_queue;
}

// Debug function to print out content of a buffer (usually a queue).
void debug_print_memory(const char *debug_str, cl_mem mem, int count) {
#ifdef DEBUG
    int err;

    // Read back the buffer from the device
    uint32_t *content = new uint32_t[count];
    assert( content );
    DEBUGF("Read back buffer from the device for debug output...\n");
    err = clEnqueueReadBuffer( commands, mem, CL_TRUE, 0, sizeof(int) * count, content, 0, NULL, NULL );
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to read buffer for debug output! %d\n", err);
        exit(1);
    }

    // Output content
    DEBUGF(debug_str);
    for(int i = 0; i < count; i++) {
      DEBUGF("0x%x ", content[i]);
    }

    DEBUGF("\n");

    delete content;
#endif
}

// Copy back the content of a queue from the GPU.
uint32_t *queue_gpu_to_cpu(cl_mem gpu_queue, uint32_t *cpu_queue, int count) {
  int err;

  // Read back the buffer from the device
  if (cpu_queue == 0)
    cpu_queue = new uint32_t[count];

  assert( cpu_queue );

  err = clEnqueueReadBuffer( commands, gpu_queue, CL_TRUE, 0, sizeof(int) * count, cpu_queue, 0, NULL, NULL );
  if (err != CL_SUCCESS)
  {
      printf("Error: Failed to read back queue from GPU to CPU! %d\n", err);
      exit(1);
  }

  //IF WE EVER GO BACK TO THE CPU, THIS BREAKS!
  cpu_queue[0] = cpu_queue[0] % QUEUE_SIZE;
  cpu_queue[1] = cpu_queue[1] % QUEUE_SIZE;

  return cpu_queue;
}

// Copy queue content to the GPU.
void queue_cpu_to_gpu(cl_mem gpu_queue, uint32_t *cpu_queue, int count) {
  int err;

  err = clEnqueueWriteBuffer( commands, gpu_queue, CL_TRUE, 0, sizeof(int) * count, cpu_queue, 0, NULL, NULL );
  if (err != CL_SUCCESS)
  {
      printf("Error: Failed to read back queue from GPU to CPU! %d\n", err);
      exit(1);
  }
}

int init_opencl(void) {
  if( g_cleanup.cleanup_needed ){
    //This already ran!
    return 0;
  }
  struct timespec timestart, timeend;
  clock_gettime(CLOCK_MONOTONIC, &timestart);

  DEBUGF("Initializing OpenCL...\n");
    int err;                            // error code returned from api calls

    // AMD specific code
    DEBUGF("Getting platform and device...\n");
    cl_platform_id platform_ids[10];
    if (clGetPlatformIDs(10, &platform_ids[0], &num_of_platforms)!= CL_SUCCESS) {
      DEBUGF("Unable to get platform_id\n");
      return 1;
    }
    assert(num_of_platforms >= 1);
    platform_id = platform_ids[0];

    // Try to get a supported GPU device
    cl_device_id device_ids[10];
    if (clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 10, &device_ids[0], &num_of_devices) != CL_SUCCESS) {
      DEBUGF("Unable to get device_id\n");
      return 1;
    }
    assert(num_of_devices >= 1);
    device_id = device_ids[0];

    DEBUGF("Num platforms/devices: %d, %d\n", num_of_platforms, num_of_devices);

    //oclPrintDevInfo(LOGCONSOLE, device_id);

    // Context properties list - must be terminated with 0
    cl_context_properties properties[3];
    properties[0]= CL_CONTEXT_PLATFORM;
    properties[1]= (cl_context_properties) platform_id;
    properties[2]= 0;

    // Create a context with the GPU device
    context = clCreateContext(properties,1,&device_id,NULL,NULL,&err);
      
    // Create a command commands
    DEBUGF("Create a command commands...\n");
    commands = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &err);
    if (!commands)
    {
        printf("Error: Failed to create a command commands!\n");
        return EXIT_FAILURE;
    }

    // Create the compute program from the source buffer
    DEBUGF("Create the compute program from the source buffer...\n");
    // When above this degree of parallelism, switch to GPU.
    char fallback_str[2048];
    const char* format =
      "#define CPU_FALLBACK %d\n"
      "#define USE_VECTORS %d\n"
      "#define USE_PREFIX_SUM %d\n"
      "#define USE_DIVERGENCE %d\n"
      "#define USE_BOTH_COMPUTE %d\n"
      "#define USE_LOAD_BALENCE %d\n"; 
    assert( !(get_use_both_compute() and get_cpu_fallback()) && "Fallback is currently incompatible with using two sets of queues" );
    if( get_use_load_balence() ) {
      assert( get_use_both_compute() && "Use of dual compute units required for load balencing");
    }
    snprintf(fallback_str, 2048, format, get_cpu_fallback(), 
             get_use_vectors(), get_use_prefix_sum(), 
             get_use_divergence(), get_use_both_compute(), get_use_load_balence());
    printf(fallback_str);
    size_t discard;
    const char* common = oclLoadProgSource(get_opencl_common_filename(), fallback_str, &discard); 
    if (!common)
    {
        DEBUGF("Error: Failed to load common header source code!\n");
        return EXIT_FAILURE;
    }
    size_t szKernelLength; // Byte size of kernel code (unused)
    const char* cSourceCL = oclLoadProgSource(get_opencl_kernel_filename(), common, &szKernelLength);
    if (!cSourceCL)
    {
        printf("Error: Failed to load source code!\n");
        return EXIT_FAILURE;
    }
    program = clCreateProgramWithSource(context, 1, (const char **) & cSourceCL, NULL, &err);
    if (!program)
    {
        printf("Error: Failed to create compute program!\n");
        return EXIT_FAILURE;
    }

    // Build the program executable
    DEBUGF("Build the program executable...\n");
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        size_t len;
        char buffer[2048];

        printf("Error: Failed to build program executable! (%d: %s)\n", err, print_cl_errstring(err));
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        printf("%s\n", buffer);

        char *build_log;
        size_t ret_val_size;
        (clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size));
        build_log =(char*)malloc(sizeof(char)*(ret_val_size+1));
        assert( build_log );
        (clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL));
        
        // to be carefully, terminate with \0
        // there's no information in the reference whether the string is 0 terminated or not
        build_log[ret_val_size] = '\0';
  
        printf("BUILD LOG: '%s'", "gpugc.cl");
        printf("%s\n", build_log);
        free(build_log);

        exit(1);
    }

    // Create the compute kernel in the program we wish to run
    DEBUGF("Create the compute kernel in the program we wish to run...\n");
    kernel = clCreateKernel(program, "mark_step", &err);
    if (!kernel || err != CL_SUCCESS) {
      printf("Error: Failed to create compute kernel!\n");
      exit(1);
    }

    // Get the maximum work group size for executing the kernel on the device
    DEBUGF("Get the maximum work group size for executing the kernel on the device...\n");
    err = clGetKernelWorkGroupInfo(kernel, device_id, CL_KERNEL_WORK_GROUP_SIZE, 
				   sizeof(native_work_group_size), &native_work_group_size, NULL);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failed to retrieve kernel work group info! %d\n", err);
        exit(1);
    }

    //TODO: Query this!
    native_compute_units = 2;
    
    g_cleanup.cleanup_needed = true;

    clock_gettime(CLOCK_MONOTONIC, &timeend);
    uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
    TRACEF("OpenCL Startup time (ms): %f\n", timeElapsed * 1.0e-6f); 

    g_stat_manager.startup = timeElapsed;
    return 0;
}

int shutdown_opencl(void) {
  struct timespec timestart, timeend;
  clock_gettime(CLOCK_MONOTONIC, &timestart);
  DEBUGF("Shutting down OpenCL...\n");  
  if( g_root_data ) {
    free(g_root_data);
    g_root_data = NULL;
  }
  if( g_output_queue ) {
    free(g_output_queue);
    g_output_queue = NULL;
  }
  // Shutdown and cleanup
  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(commands);
  clReleaseContext(context);

  g_cleanup.cleanup_needed = false;

  clock_gettime(CLOCK_MONOTONIC, &timeend);
  uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
  TRACEF("OpenCL shutdown time (ms): %f\n", timeElapsed * 1.0e-6f); 

  g_stat_manager.shutdown = timeElapsed;
  return 0;
}


struct frontier_queue g_queue;


inline uint32_t queue_length(uint32_t *q) {
  const uint32_t qs = q[0];
  const uint32_t qe = q[1];
  //handle the wrap around case
  return ((qe + QUEUE_SIZE - qs) % QUEUE_SIZE);
}
inline uint32_t queue_length(frontier_queue* q) {
  //memory wise, these are the same
  return queue_length((uint32_t*)q);
}

inline void populate_queue(void *head, void *tail) {
  g_queue.qs = 0;
  g_queue.qe = 0;
  assert(ULONG_MAX > QUEUE_SIZE);
  for (struct refgraph_entry **a = (struct refgraph_entry **)head; a != tail; a++) {
    g_queue.q[g_queue.qe++] = *a;
    if (g_queue.qe == QUEUE_SIZE) {
      printf("queue full\n"); 
      abort(); }
  }
  assert( g_queue.qe >= g_queue.qs );
}


void cpu_mark(struct frontier_queue* queue);

// Fallback function to perform a mark step on the CPU.
inline static void cpu_mark_step(uint32_t *q1, uint32_t *q2) {
  while (q1[0] != q1[1]) {
    struct refgraph_entry *re = (struct refgraph_entry*)q1[2+q1[0]++];
    DEBUGF("%p\n", re);
    if( re == NULL ) continue;
    if (q1[0] == QUEUE_SIZE) q1[0] = 0;
    if (re->num_edges & VISITED)
      continue;

    int num_edges = re->num_edges & 0x3fffffff;
    DEBUGF("Processing %p (object %p, %d refs) [Queue size %d]\n", re, re->object, num_edges, queue_length(q1));
    //show_type(re->object);
    for (uint32_t i = 0; i < num_edges; i++) {
      if (re->edges[i] != NULL) {
        DEBUGF("  Reference %d of %d: %p\n", i, re->num_edges, re->edges[i]);
        q2[2+q2[1]++] = (uint32_t)re->edges[i];
        
        if (q2[1] == QUEUE_SIZE) q2[1] = 0;
        if (q2[0] == q2[1]) { 
          printf("queue full\n"); 
          abort();
        }

      }
    }
    re->num_edges |= VISITED; //mark the object.
  }
}







// Do the mark phase on the GPU.
// Returns 1 iff data has been copied back from the GPU already.
int gc(cl_mem *space, void *space_begin, uint32_t space_size, void *roots_start, int length) {
    int err;

    DEBUGF("Create space buffer on the gpu...\n");

    // It may seem like it would be faster to save the buffer or memory around, but it's actually not.  The 
    // kernel expects to modify this section many times over.  As such, we need to perform the copy regardless.  
    // Given that, creating the buffer itself is a cheap operation.  It may be worth revisting the design to
    // make the root set immutable and pass the bounds separately.  Would require more complicated OpenCL code,
    // but might be faster due to caching effects.  It would also allow a zero copy init here.  (i.e. no memcopy)

    DEBUGF("Write the queue structure to be moved to the GPU\n");
    uint32_t *root_data = get_root_data_memory(); //saved accross calls for the speed, deleted on process exit
    memset(root_data, get_queue_alloc_size_in_bytes(), 0);

    if( get_use_both_compute() ) {
      root_data[0] = (uint32_t)0; // Start of the queue.
      root_data[1] = (uint32_t)length/2; // End of the queue
      memcpy(root_data+2, roots_start, length/2*sizeof(uint32_t));
      
      root_data[0+QUEUE_SIZE+2] = (uint32_t)0; // Start of the queue.
      root_data[1+QUEUE_SIZE+2] = (uint32_t)(length-length/2); // End of the queue
      memcpy(root_data+2+QUEUE_SIZE+2, (uint32_t*)roots_start+length/2, (length-length/2)*sizeof(uint32_t));
    } else {
      //single_queue
      root_data[0] = (uint32_t)0; // Start of the queue.
      root_data[1] = (uint32_t)length; // End of the queue
      memcpy(root_data+2, roots_start, length*sizeof(uint32_t));
    }

    //This could be more elegantly handled, but if the data set
    // is too small, we don't want to setup the kernel or ever
    // execute on the GPU
    //
    // I think this is flawed - imagine you have a big tree: there
    // is only one root in the root set, but you still want to execute
    // it on the GPU. As long as we don't switch to the GPU as soon as a
    // sufficient level of parallelism is reached, just using the size of
    //  the root set to deduce the overall size of the collection is dangerous.

    /*if( queue_length(root_data) < GPU_FALLBACK ) {
      //FLAGIT
      cpu_mark((struct frontier_queue*)root_data);
      return 1;
    }*/

    DEBUGF("Create queue buffer on the gpu...\n");
    // Create a kernel read/write block for use in the queue. As writen this will make sure
    // the contents of root_data are available on the GPU in an implementation defined maner.  
    // It may copy, it may be dual addressable.  It appears the auto-tune option here is our
    // best bet.  Using CL_MEM_COPY_HOST_PTR slows us down by a factor of 7x and 
    // CL_MEM_ALLOC_HOST_PTR I couldn't get working cleanly.
    cl_mem queue = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
				  get_queue_alloc_size_in_bytes(), root_data, &err);
    if (err != CL_SUCCESS)
    {
        printf("error: failed to create buffer! %d (%s)\n", err);
        exit(1);
    }


    // Create a kernel read block for use by the kernel as a second queue.  This does
    // not need to be host readable or initialized
    // PERF: This can easily be extracted out to global to avoid recreation on each call. Is it worth doing so?
    DEBUGF("Create second buffer on the gpu...\n");
    //make sure our output queue is setup (before the kernel starts)
    uint32_t* queue2_mem = get_output_queue_memory();
    memset(queue2_mem, get_queue_alloc_size_in_bytes(), 0);
    queue2_mem[0] = 0;
    queue2_mem[1] = 0;
    cl_mem queue2 = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                   get_queue_alloc_size_in_bytes(), queue2_mem, &err);
    if (err != CL_SUCCESS)
    {
      printf("error: failed to create buffer! %d\n", err);
      exit(1);
    }

    uint32_t zero = 0;
    cl_mem lock_counter = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 
                                         sizeof(uint32_t), &zero, &err);
    if (err != CL_SUCCESS)
    {
      printf("error: failed to create buffer! %d\n", err);
      exit(1);
    }

    uint32_t zero_copy = 0;
    cl_mem lock_counter_copy = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 
                                         sizeof(uint32_t), &zero_copy, &err);
    if (err != CL_SUCCESS)
    {
      printf("error: failed to create buffer! %d\n", err);
      exit(1);
    }


    // Set the arguments to our compute kernel
    DEBUGF("Set the arguments to our compute kernel...\n");
    err = 0;
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), space);
    err |= clSetKernelArg(kernel, 1, sizeof(uint32_t)*BUFFER_SIZE, NULL); //TODO: Flexibility about maximum object size.
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &queue);
    err |= clSetKernelArg(kernel, 3, sizeof(cl_mem), &queue2);
    err |= clSetKernelArg(kernel, 4, sizeof(uint32_t), &space_begin);
    err |= clSetKernelArg(kernel, 5, sizeof(uint32_t)*(300), NULL); //scratch space
    if (err != CL_SUCCESS)
    {
      printf("Error: Failed to set kernel arguments! %d\n", err);
      exit(1);
    }

    // Event to measure execution time.
    cl_event event;

    // Maybe this is how we should handle the two queues?
    size_t local = get_serial_flag() ? 1 : native_work_group_size;
    size_t global = get_serial_flag() ? 1 
      : (get_use_both_compute() ? native_compute_units*local : local);

    // Copies of the queues on the CPU-side: only filled in when switching back to the CPU.
    uint32_t *cpu_queue1 = 0;
    uint32_t *cpu_queue2 = 0;

    // Is the data back on the CPU and does not have to be copied again?
    int data_on_cpu = 0;

    // If the degree of parallelism is too low, switch back to CPU.
    // Either at the beginning of the mark phase or we need to switch back to the GPU from the CPU.
    
    // Execute the kernel over the entire range of our 1d input data set
    DEBUGF("Execute the baseline GC kernel...\n");
    err = clEnqueueNDRangeKernel(commands, kernel, 1, NULL, &global, &local, 0, NULL, &event);
    if (err)
      {
        printf("Error: Failed to execute kernel! (%d: %s)\n", err, print_cl_errstring(err));
        exit(1);
      }
    
    // Wait for the command commands to get serviced before reading back results
    DEBUGF("Wait for the command commands to get serviced before reading back results...\n");
    err = clFinish(commands);
    if (err) {
      printf("Error: Failed to complete kernel! (%d: %s)\n", err, print_cl_errstring(err));
      exit(1);
    }
    
    // Copy back the queues.
    cpu_queue1 = queue_gpu_to_cpu(queue, cpu_queue1, 2+QUEUE_SIZE);
    cpu_queue2 = queue_gpu_to_cpu(queue2, cpu_queue2, 2+QUEUE_SIZE);
    
    cl_ulong submit, queued, start, end; 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submit, NULL); 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, NULL); 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL); 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL); 
    g_stat_manager.stats.back().gpu_executionTimeInMilliseconds = (end - start) * 1.0e-6f;
    g_stat_manager.stats.back().gpu_submitTimeInMilliseconds = (end - submit) * 1.0e-6f;
    g_stat_manager.stats.back().gpu_queueTimeInMilliseconds = (end - queued) * 1.0e-6f;
    TRACEF("GPU Kernel Execution time (ms): %f\n", g_stat_manager.stats.back().gpu_executionTimeInMilliseconds);
    TRACEF("GPU Kernel Submit Overhead time (ms): %f\n", 
           g_stat_manager.stats.back().gpu_submitTimeInMilliseconds - g_stat_manager.stats.back().gpu_executionTimeInMilliseconds);
    TRACEF("GPU Kernel Queue Overhead time (ms): %f\n", 
           g_stat_manager.stats.back().gpu_queueTimeInMilliseconds - g_stat_manager.stats.back().gpu_executionTimeInMilliseconds);
    
    debug_print_memory("Queue 1: ", queue, 1000);
    debug_print_memory("Queue 2: ", queue2, 1000);
    if( queue_length(cpu_queue1) != 0 || queue_length(cpu_queue2) != 0 ) {
      TRACEF("Switching to the CPU...\n");

      // Copy marking from the GPU back to the CPU.
      // TODO: Avoid copying it back a second time!
      DEBUGF("Copying back heap structure...\n");
      err = clEnqueueReadBuffer(commands, *space, CL_TRUE, 0, space_size, space_begin, 0, NULL, NULL);
      
      if (err != CL_SUCCESS) {
        printf("Error copying results from GPU: %s\n", print_cl_errstring(err));
        exit(1);
      }
      
      data_on_cpu = 1;
      
      struct timespec timestart, timeend;
      clock_gettime(CLOCK_MONOTONIC, &timestart);
      

      // Perform the mark phase on the CPU.
      uint32_t *in_queue = cpu_queue1;
      uint32_t *out_queue = cpu_queue2;
      while (queue_length(in_queue) != 0 || queue_length(out_queue) != 0) {
        cpu_mark_step(in_queue, out_queue);
        
        // TODO: Go back to the GPU if necessary.
        // For that, also need to copy memory back to GPU.
        
        // Copy back the queues.
        /*if (TODO: If not finished.) {
          queue_gpu_to_cpu(cpu_queue1, queue1, 2+QUEUE_SIZE);
          queue_gpu_to_cpu(cpu_queue2, queue2, 2+QUEUE_SIZE);
          }*/
        
        uint32_t *temp = in_queue; in_queue = out_queue; out_queue = temp;
      }
      clock_gettime(CLOCK_MONOTONIC, &timeend);
      
      uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
      g_stat_manager.stats.back().cpu_time = timeElapsed * 1.0e-6f;
    }

    delete cpu_queue1;
    cpu_queue1 = NULL;
    delete cpu_queue2;
    cpu_queue2 = NULL;

    clReleaseMemObject(queue);
    clReleaseMemObject(queue2);
    clReleaseMemObject(lock_counter);
    clReleaseMemObject(lock_counter_copy);

    return data_on_cpu;
}



void show_type(void *object) {
  void *tib = PTR_AT(object, -12);
  DEBUGF("tib=%p\n", tib);
  void *type = PTR_AT(tib, 0);
  void *typeRef = PTR_AT(type, 4);
  void *name = PTR_AT(typeRef, 0);
  void *name_val = PTR_AT(name, 0);
  DEBUGF("  Type [%.*s]\n", UINT_AT(name_val, -4), (char *)name_val);
}



#define HUGE_SIZE (256*1024*1024)
volatile unsigned int huge_memory_region[HUGE_SIZE];

void force_cpu_memory_flush() {
  for(uint32_t i = 0; i < HUGE_SIZE; ++i) {
    huge_memory_region[i] = 'a';
  }
}


/* Description of parameters from Jikes (and thus our tests as well).

At the moment, the elements are pointers into the space - the OpenCL kernel translates them into 32-bit word offsets during execution, since this is how it has to access the elements.

The bitfields within the heap structure are as follows:
* 32 bit: pointer to the original object
* 32 bit: header (1 mark bit, 15 bit unused, 16 bit number of references)
* For each reference: one 32-bit pointer into the heap structure.
*/
void validate_structure(void* p_roots_begin, void* p_roots_end, void* p_space_begin, void* p_space_end) {
  //basic sanity check
  assert_read_write(p_roots_begin, p_roots_end);    
  assert_read_write(p_space_begin, p_space_end);    
  //convert everything to the appropriate types
  // The root structure is an array of pointers to refgraph_entries
  struct refgraph_entry ** roots_begin = (struct refgraph_entry **)p_roots_begin;
  struct refgraph_entry ** roots_end = (struct refgraph_entry **)p_roots_end;
  // The space is an array of refgraph_entries
  struct refgraph_entry* space_begin = (struct refgraph_entry *)p_space_begin;
  struct refgraph_entry* space_end = (struct refgraph_entry *)p_space_end;
  
  //Check that all root pointers point to something in the memory space given
  // NOTE: NULL is _explicitly_ not allow here
  for (struct refgraph_entry **a = roots_begin; a != roots_end; a++) {
    assert( (void*)(*a) >= p_space_begin && (void*)(*a) < p_space_end );
  }

#if 0 
  //This code makes unfounded assumptions about the layout of objects within the space.  As such, it's essentially worthless unless you've hand constructed examples.
  struct refgraph_entry *cur = space_begin;
  while(cur < space_end) {
    //reminder: the entire object may be null (i.e. past the end of a real block)

    //objects must _not_ be in the space
    assert( cur->object < p_space_begin || cur->object >= p_space_end );

    //The visited flags must not be set (yet)
    //assert( !(cur->num_edges & (VISITED|VISITED2)) );

    const uint32_t num_edges = cur->num_edges & 0x3fffffff;
    for (uint32_t i = 0; i < num_edges; i++) {
      refgraph_entry *e = cur->edges[i];
      if( e != NULL ) {
        //edges must be in the space
        assert( (void*)(e) >= p_space_begin && (void*)(e) < p_space_end );
      }
    }
    //advance to the start of the new object
    //consider padding at end of object
    
    //current code does no alignment
    int obj_size = ((2 + num_edges) % 4 == 0) ? (2 + num_edges) : (((2 + num_edges) & (~0x3)) + 4);
    cur = (struct refgraph_entry *) ((uint32_t*)cur + obj_size);
  }
#endif
}

//Operates on the prepopulated queue and modifies the refgraph!
//Sets ONLY the VISITED flag, not the VISITED2 flag.  (As with the 
// GPU impl)
void cpu_mark(struct frontier_queue* queue) {
  //This function doesn't use the cpu_mark_step, but that's mostly an artifact of history.  Might be worth combining them.
  struct timespec timestart, timeend;
  clock_gettime(CLOCK_MONOTONIC, &timestart);
  int marked = 0;
  while (queue->qs != queue->qe) {
    struct refgraph_entry *re = queue->q[queue->qs++];
    DEBUGF("%p\n", re);
    if( re == NULL ) continue;
    if (queue->qs == QUEUE_SIZE) queue->qs = 0;
    if (re->num_edges & VISITED)
      continue;

    int num_edges = re->num_edges & 0x3fffffff;
    DEBUGF("Processing %p (object %p, %d refs) [Queue size %d]\n", re, re->object, num_edges, queue_length(queue));
    //show_type(re->object);
    for (uint32_t i = 0; i < num_edges; i++) {
      if (re->edges[i] != NULL) {
        DEBUGF("  Reference %d of %d: %p\n", i, re->num_edges, re->edges[i]);
        queue->q[queue->qe++] = re->edges[i];
        if (queue->qe == QUEUE_SIZE) queue->qe = 0;
        if (queue->qe == queue->qs) { 
          printf("queue full\n"); 
          abort();
        }
      }
    }
    re->num_edges |= VISITED; //needed for the map back code

    marked++;
  }
  clock_gettime(CLOCK_MONOTONIC, &timeend);


  uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
  g_stat_manager.stats.back().cpu_time = timeElapsed * 1.0e-6f;
  TRACEF("CPU Mark Execution time (ms): %f\n", timeElapsed * 1.0e-6f);
  TRACEF("%d objects marked\n", marked);
}

/// This is the interface called (indirectly) from JIKES
void launch_checked_opencl_mark(void *head, void *tail, void *start, void *end) {

  if( !g_stat_manager.stream.is_open() ) {
    g_stat_manager.stream.open(get_environment_variable("gpugc_stats_file", "stats.dat").c_str());
  }
  g_stat_manager.stats.push_back(stat_entry());
  printf("Collection %d\n", g_stat_manager.stats.size()-1);

  struct timespec timestart, timeend;
  clock_gettime(CLOCK_MONOTONIC, &timestart);


  TRACEF("\n===================================================\n");
  if( get_validate_input_flag() ) {
    //Make sure the data structures passed in are correct
    printf("Validating input data...");
    validate_structure(head, tail, start, end);
  }
  DEBUGF("Trace Queue %p -> %p: reference graph %p to %p\n", head, tail, start, end);
  DEBUGF("===================================================\n");

  /*FILE *f = fopen("refs-dump.out", "wb");
  fwrite(start, (char *)end - (char *)start, 1, f);
  fclose(f);*/

  //Copy the root set into the global queue
  populate_queue(head, tail);
  assert( g_queue.qe >= g_queue.qs );

  //Record the size of the space in bytes for latter reference
  g_stat_manager.stats.back().mem_size = (char*)end-(char*)start;

  const ExecutionMode mode = get_execution_mode();
  switch(mode) {
  //Heap Structure Analysis
  case e_Analysis: {
    TRACEF("Analyzing heap structure\n");
    analyze_heap_structure(g_queue.q, g_queue.qs, g_queue.qe);
    assert(g_queue.qe == g_queue.qs);
    break;
  }
  //CPU Only Profiling
  case e_CPUOnly: {
    cpu_mark(&g_queue);
    assert(g_queue.qe == g_queue.qs);
    break;
  }
  //GPU Only Profiling
  case e_GPUOnly: {
    // Perform the mark on the GPU.
    if( 0 != launch_opencl_mark(head, tail, start, end) ) {
      printf("opencl mark failed\n"); 
      abort();
    }

    DEBUGF("After opencl mark\n");
    break;
  }
  case e_Auto: {
    assert(false); //inmplemented
  }
  default: {
    assert(false);
    break;
  }
  };

#ifndef PROFILE
  //Force a cache flush
  DEBUGF("Forcing a flush of the CPU cache\n");
  force_cpu_memory_flush();
#endif

  //check out result using our CPU implementation?
  const bool checked = get_checked_flag();
  if( checked ) {

    //Copy the root set into the global queue
    //Again... Need to reset the queue
    populate_queue(head, tail);
    assert( g_queue.qe >= g_queue.qs );


    // Check the number of marked objects.
    DEBUGF("Checking correctness of the marking...\n");
    struct timespec timestart, timeend;
    clock_gettime(CLOCK_MONOTONIC, &timestart);
    int marked = 0, gpu_marked = 0;
    while (g_queue.qs != g_queue.qe) {
      struct refgraph_entry *re = g_queue.q[g_queue.qs++];
      DEBUGF("%p\n", re);
      if( re == NULL ) continue;
      if (g_queue.qs == QUEUE_SIZE) g_queue.qs = 0;
      if (re->num_edges & VISITED2)
        continue;

      int num_edges = re->num_edges & 0x3fffffff;
      DEBUGF("Processing %p (object %p, %d refs) [Queue size %d]\n", re, re->object, num_edges, queue_length(&g_queue));
      //show_type(re->object);
      for (uint32_t i = 0; i < num_edges; i++) {
        if (re->edges[i] != NULL) {
          DEBUGF("  Reference %d of %d: %p\n", i, re->num_edges, re->edges[i]);
          g_queue.q[g_queue.qe++] = re->edges[i];
          if (g_queue.qe == QUEUE_SIZE) g_queue.qe = 0;
          if (g_queue.qe == g_queue.qs) { 
            printf("queue full\n"); 
            abort();
          }
        }
      }
      re->num_edges |= VISITED2;

      if (re->num_edges & VISITED)
        gpu_marked++;

      marked++;
    }
    clock_gettime(CLOCK_MONOTONIC, &timeend);


    uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
    g_stat_manager.stats.back().check_time = timeElapsed * 1.0e-6f;

    TRACEF("CPU Mark Execution time (ms): %f\n", timeElapsed * 1.0e-6f);
    TRACEF("%d/%d objects marked\n", gpu_marked, marked);

    if( marked != gpu_marked ) {
      assert( marked == gpu_marked );
    }
  }

  if( get_validate_output_flag() ) {
    //Make sure the data structures passed in are correct
    printf("Validating output data...");
    validate_structure(head, tail, start, end);
  }

  TRACEF("===================================================\n\n");
  
  clock_gettime(CLOCK_MONOTONIC, &timeend);
  uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
  g_stat_manager.stats.back().overall = timeElapsed * 1.0e-6f;
}

int launch_opencl_mark(void *head, void *tail, void *start, void *end) {
	if (init_opencl() != 0)
	  return 1;

	int error;

	// Allocate space for heap structure.
	DEBUGF("Preparing buffer for heap structure...\n");
	//IN BYTES
	const uint32_t bufSize = ((uint32_t*)end - (uint32_t*)start)*sizeof(uint32_t*);
	DEBUGF("Buffer size: %d\n", bufSize);

	//Note: It may seem tempting to remove the memcpy here and use a CL_MEM_USE_HOST_PTR
	// mapped block to transfer the memory directly to the GPU without a copy, but this
	// is not viable.  JIKES uses a custom copy-on-write allocation scheme which OpenCL
	// does not play well with.  Because of this, we need to use a more "traditional" 
	// copy to a dedicated buffer.  Once it's in the dedicated buffer, we can let 
	// OpenCL decide to move that buffer accross.  This is arguable better since
	// we're no long dictating when that move must happen.
	// TODO: Reuse last malloc block if big enough?
	uint32_t *malloc_space = (uint32_t*)malloc(bufSize);
    assert( malloc_space );
	memcpy(malloc_space, start, bufSize);


	// Create a kernel read/write block for use in the queue. As writen this will make sure
	// the contents of malloc_space are available on the GPU in an implementation defined maner.  
	// It may copy, it may be dual addressable.  It appears the auto-tune option here is our
	// best bet.  Using CL_MEM_COPY_HOST_PTR slows us down by a factor of 7x and 
	// CL_MEM_ALLOC_HOST_PTR I couldn't get working cleanly.
	cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
				       bufSize, malloc_space, &error);
	if (error != CL_SUCCESS) {
	  printf("error: failed to create buffer! %d (%s)\n", error);
	  exit(1);
	}

	// Do the mark phase on the GPU.
	uint32_t roots_length = ((uint32_t*)tail - (uint32_t*)head);
	DEBUGF("Number of roots: %d\n", roots_length);
	
        if(gc(&buffer, start, bufSize, head, roots_length) == 0) {
	  // Copying heap structure back from the GPU (now containing the marking).  
	  // By experimental testing, writting to JIKES copy-on-write allocated memory
	  // appears to work just fine.
	  DEBUGF("Copying back heap structure...\n");
	  //PERF: For overall results, this command could be queued with the kernel itself.  For our
	  // micro benchmark of the kernel itself, we don't want to do this though.
	  error = clEnqueueReadBuffer(commands, buffer, CL_TRUE, 0, bufSize, start, 0, NULL, NULL);
	
	  if (error != CL_SUCCESS) {
	    printf("Error copying results from GPU: %s\n", print_cl_errstring(error));
	    return 1;
	  }
        }

	clReleaseMemObject(buffer);
	free(malloc_space);

	return 0;

}

void reset_harness() {
  printf("\n\n\n ---- RESET FUN CALLED ---- \n\n\n");

  //clear anything done so far
  g_stat_manager.stream.close();
  g_stat_manager.stats.clear();
}

