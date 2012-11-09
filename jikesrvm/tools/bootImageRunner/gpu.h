#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "opencl/gpugc.h"
#include "opencl/common.h"

extern "C" void
sysGPUGCtraceQueue(void *head, void *tail, void *start, void *end) {
  //Proxy to the implementation in opencl/gpugc.h for ease of testing
  launch_checked_opencl_mark(head, tail, start, end);
}

extern "C" void
sysGPUGCreset() {
  reset_harness();
}

