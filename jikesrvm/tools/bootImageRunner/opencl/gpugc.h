/** This file defines the public API that is called by Jikes */

#ifndef GPUGC_H
#define GPUGC_H

/** This is the entry point to be called from Jikes */
void launch_checked_opencl_mark(void *head, void *tail,
                                void *start, void *end);

void reset_harness();

#endif //GPUGC_H
