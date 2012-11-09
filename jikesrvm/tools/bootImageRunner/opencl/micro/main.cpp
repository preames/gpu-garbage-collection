
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

#include <CL/cl.h>
#include <CL/cl_ext.h>

#include "gpugc-utils.h"

const char *cPathAndName = "micro.cl"; //"tools/bootImageRunner/opencl/gpugc.cl";

// AMD specific
cl_platform_id platform_id;
cl_uint num_of_platforms=0;
cl_uint num_of_devices=0;

cl_device_id device_id;             // compute device id 
cl_context context;                 // compute context
cl_command_queue commands;          // compute command queue
cl_program program;                 // compute program

#if 0
#define DEBUGF printf
#else
#define DEBUGF //printf
#endif
#define PROFILE
//#define PROFILE_GPU_ONLY //graphics
//#define PROFILE_CPU_ONLY //traditional

int64_t nanosecs(struct timespec* time) {
  int64_t nano = (time->tv_sec * 1000000000);
  nano += time->tv_nsec;
  return nano;
}
int64_t timespecDiff(struct timespec *timeA_p, struct timespec *timeB_p)
{
  return nanosecs(timeA_p) - nanosecs(timeB_p);
}

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
    if( cleanup_needed ) {
      shutdown_opencl();
      cleanup_needed = false;
    }
  }
} g_cleanup;

void debug_print_memory(const char *debug_str, cl_mem mem, int count) {
#ifdef DEBUG
    int err;

    // Read back the buffer from the device
    uint32_t *content = new uint32_t[count];
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
    size_t szKernelLength; // Byte size of kernel code (unused)
    const char* cSourceCL = oclLoadProgSource(cPathAndName, "", &szKernelLength);
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
        (clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL));
        
        // to be carefully, terminate with \0
        // there's no information in the reference whether the string is 0 terminated or not
        build_log[ret_val_size] = '\0';
  
        printf("BUILD LOG: '%s'", "gpugc.cl");
        printf("%s\n", build_log);
        free(build_log);

        exit(1);
    }

    g_cleanup.cleanup_needed = true;

    clock_gettime(CLOCK_MONOTONIC, &timeend);
    uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
    printf("OpenCL Startup time (ms): %f\n", timeElapsed * 1.0e-6f); 
    return 0;
}

int shutdown_opencl(void) {
  struct timespec timestart, timeend;
  clock_gettime(CLOCK_MONOTONIC, &timestart);
  DEBUGF("Shutting down OpenCL...\n");  
  // Shutdown and cleanup
  clReleaseProgram(program);
  clReleaseCommandQueue(commands);
  clReleaseContext(context);

  g_cleanup.cleanup_needed = false;

  clock_gettime(CLOCK_MONOTONIC, &timeend);
  uint64_t timeElapsed = timespecDiff(&timeend, &timestart); //nanoseconds
  printf("OpenCL shutdown time (ms): %f\n", timeElapsed * 1.0e-6f); 
  return 0;
}

struct kernel_t {
  std::string name;
  cl_kernel kernel;                   // compute kernel
  kernel_t(std::string kernel_name) 
    : name(kernel_name) {
    DEBUGF("Create the compute kernel in the program we wish to run...\n");
    int err = CL_SUCCESS;
    kernel = clCreateKernel(program, "empty_kernel", &err);
    if (!kernel || err != CL_SUCCESS) {
      printf("Error: Failed to create compute kernel!\n");
      exit(1);
    }
  }
  ~kernel_t() {
    if( kernel ) {
      clReleaseKernel(kernel);
    }
  }

  void execute() {
    int err = CL_SUCCESS;
    cl_event event;
    size_t local = 256;
    size_t global = 256;
    err = clEnqueueNDRangeKernel(commands, kernel, 1, NULL, &global, &local, 0, NULL, &event);
    if (err) {
      printf("Error: Failed to execute kernel! (%d: %s)\n", err, print_cl_errstring(err));
      exit(1);
    }
    
    // Wait for the command commands to get serviced before reading back results
    DEBUGF("Wait for the command commands to get serviced before reading back results...\n");
    clFinish(commands);
    
    cl_ulong submit, queued, start, end; 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submit, NULL); 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, NULL); 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL); 
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL); 
    float executionTimeInMilliseconds = (end - start) * 1.0e-6f;
    float submitTimeInMilliseconds = (end - submit) * 1.0e-6f;
    float queueTimeInMilliseconds = (end - queued) * 1.0e-6f;
    printf("GPU Kernel Execution time (ms): %f\n", executionTimeInMilliseconds);
    printf("GPU Kernel Submit Overhead time (ms): %f\n", 
           submitTimeInMilliseconds - executionTimeInMilliseconds);
    printf("GPU Kernel Queue Overhead time (ms): %f\n", 
           queueTimeInMilliseconds - executionTimeInMilliseconds);
  }
};

void ptrchase_exec() {
  int err = CL_SUCCESS;

  cl_kernel kernel;                   // compute kernel
  kernel = clCreateKernel(program, "volatile_latency_kernel", &err);
  if (!kernel || err != CL_SUCCESS) {
    printf("Error: Failed to create compute kernel!\n");
    exit(1);
  }
  
  const int bufSize = 63*1024*1024;
  uint32_t *malloc_space = (uint32_t*)malloc(bufSize);
  assert( malloc_space );
  memset(malloc_space, '0', bufSize);
  for(int i = 0; i < bufSize/sizeof(unsigned int); i++) {
    malloc_space[i] = rand() % bufSize;
  }

  cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                 bufSize, malloc_space, &err);
  if (err != CL_SUCCESS) {
    printf("error: failed to create buffer! %d (%s)\n", err);
    exit(1);
  }

  err |= clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer);
  if (err != CL_SUCCESS) {
    printf("error: failed to create buffer! %d (%s)\n", err);
    exit(1);
  }

  cl_event event;
  size_t local = 256;
  size_t global = 512;
  err = clEnqueueNDRangeKernel(commands, kernel, 1, NULL, &global, &local, 0, NULL, &event);
  if (err) {
    printf("Error: Failed to execute kernel! (%d: %s)\n", err, print_cl_errstring(err));
    exit(1);
  }
    
  // Wait for the command commands to get serviced before reading back results
  DEBUGF("Wait for the command commands to get serviced before reading back results...\n");
  clFinish(commands);
    
  cl_ulong submit, queued, start, end; 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submit, NULL); 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, NULL); 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL); 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL); 
  float executionTimeInMilliseconds = (end - start) * 1.0e-6f;
  float submitTimeInMilliseconds = (end - submit) * 1.0e-6f;
  float queueTimeInMilliseconds = (end - queued) * 1.0e-6f;
  printf("GPU Kernel Execution time (ms): %f\n", executionTimeInMilliseconds);
  printf("GPU Kernel Submit Overhead time (ms): %f\n", 
         submitTimeInMilliseconds - executionTimeInMilliseconds);
  printf("GPU Kernel Queue Overhead time (ms): %f\n", 
           queueTimeInMilliseconds - executionTimeInMilliseconds);

  clReleaseMemObject(buffer);
  free(malloc_space);

  if( kernel ) {
    clReleaseKernel(kernel);
  }
}

void mem1gb_exec() {
  int err = CL_SUCCESS;

  cl_kernel kernel;                   // compute kernel
  kernel = clCreateKernel(program, "mem1gb_kernel", &err);
  if (!kernel || err != CL_SUCCESS) {
    printf("Error: Failed to create compute kernel!\n");
    exit(1);
  }

  for(int i = 0; i < 2; i += 1) {
    const int bufSize = 1*1024*1024;
    printf("Trying to allocate: %d bytes\n", bufSize);
    int err = CL_SUCCESS;

    uint32_t *malloc_space = (uint32_t*)malloc(bufSize);
    assert( malloc_space );
    memset(malloc_space, '0', bufSize);
    cl_mem buffer = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                   bufSize, malloc_space, &err);
    if (err != CL_SUCCESS) {
      printf("error: failed to create buffer! %d (%s)\n", err);
      exit(1);
    }

    err |= clSetKernelArg(kernel, i, sizeof(cl_mem), &buffer);
    if (err != CL_SUCCESS) {
      printf("error: failed to create buffer! %d (%s)\n", err);
      exit(1);
    }
  }

  


  cl_event event;
  size_t local = 1;
  size_t global = 1;
  err = clEnqueueNDRangeKernel(commands, kernel, 1, NULL, &global, &local, 0, NULL, &event);
  if (err) {
    printf("Error: Failed to execute kernel! (%d: %s)\n", err, print_cl_errstring(err));
    exit(1);
  }
    
  // Wait for the command commands to get serviced before reading back results
  DEBUGF("Wait for the command commands to get serviced before reading back results...\n");
  clFinish(commands);
    
  cl_ulong submit, queued, start, end; 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &submit, NULL); 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &queued, NULL); 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL); 
  clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL); 
  float executionTimeInMilliseconds = (end - start) * 1.0e-6f;
  float submitTimeInMilliseconds = (end - submit) * 1.0e-6f;
  float queueTimeInMilliseconds = (end - queued) * 1.0e-6f;
  printf("GPU Kernel Execution time (ms): %f\n", executionTimeInMilliseconds);
  printf("GPU Kernel Submit Overhead time (ms): %f\n", 
         submitTimeInMilliseconds - executionTimeInMilliseconds);
  printf("GPU Kernel Queue Overhead time (ms): %f\n", 
           queueTimeInMilliseconds - executionTimeInMilliseconds);

  //clReleaseMemObject(buffer);
  //free(malloc_space);

  if( kernel ) {
    clReleaseKernel(kernel);
  }
}



int main(int argc, char **argv) {
  printf("Starting microbenchmarks\n");

  init_opencl();

  kernel_t empty_kernel("empty_kernel");
  // Create the compute kernel in the program we wish to run
  for(int i = 0; i < 20; i++) {
    //ptrchase_exec();
    //empty_kernel.execute();
  }
  mem1gb_exec();


  shutdown_opencl();
}
