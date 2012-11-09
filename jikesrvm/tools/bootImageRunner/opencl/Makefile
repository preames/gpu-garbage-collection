# NOTE: This makefile is only for local testing and for quick builds.  It
# is not used by the actual JIKES build system (and thus our java language
# tests or benchmarks.)

# Use the 32 bit version!
AMD_LIB_DIR := /opt/opt/AMD-APP-SDK/lib/x86

gpugc: libgpugc.so utils.h gpugc.h gpugc-internal.h common.h driver.cxx
	g++ -m32 -O3 -fPIC driver.cxx -L. -lgpugc -o gpugc

library: libgpugc.so
libgpugc.so: gpugc.o analyze.o utils.o gpugc-utils.o gpugc-internal.h
	g++ -m32 -O3 -shared -fPIC -lOpenCL gpugc-utils.o utils.o analyze.o gpugc.o -o libgpugc.so -lrt

utils.o: utils.h utils.cxx
	g++ -m32 -O3 -fPIC -c utils.cxx

gpugc.o: utils.h gpugc.h gpugc-internal.h common.h gpugc.c
	g++ -m32 -O3 -fPIC -c gpugc.c

analyze.o: utils.h gpugc.h gpugc-internal.h common.h analyze.cxx
	g++ -m32 -O3 -fPIC -c analyze.cxx

gpugc-utils.o: gpugc-utils.h gpugc-utils.cxx
	g++ -m32 -O3 -fPIC -c gpugc-utils.cxx


clean:
	rm -f *.o
	rm -f gpugc
	rm -f libgpugc.so


# should generate an undefined reference to main.  That's expected.
analyze: utils.cxx utils.h analyze.cxx
	g++ -O3 utils.cxx analyze.cxx -o analyze.out

