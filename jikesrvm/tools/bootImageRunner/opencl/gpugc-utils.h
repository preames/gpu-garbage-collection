
#ifndef __GPUGC_UTILS_H
#define __GPUGC_UTILS_H
// Useful utility functions from the CUDA SDK and other documentation.
//

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

// Note: Due to licensing, we can not include the functions below in a public release.  We didn't write them.
// You can find them online via Google (we did) or in the NVIDIA OpenCL SDK.




char *strdup(const char *s1) throw();

// Code based on NVIDIA's oclUtils.cpp
char* oclLoadProgSource(const char* cFilename, const char* cPreamble, size_t* szFinalLength);

// From http://forums.amd.com/forum/messageview.cfm?catid=390&threadid=128536
char *print_cl_errstring(cl_int err);

#endif // __GPUGC_UTILS_H
