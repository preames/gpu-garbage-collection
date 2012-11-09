# NOTE: This makefile is only for local testing and for quick builds.  It
# is not used by the actual JIKES build system (and thus our java language
# tests or benchmarks.)

# Use the 32 bit version!
AMD_LIB_DIR := /opt/opt/AMD-APP-SDK/lib/x86

all: 
	g++ -g -m32 -O3 -lOpenCL main.cpp -lrt -o micro

clean:
	rm -f *.o
	rm -f a.out micro
