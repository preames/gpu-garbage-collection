# This script is responsible for running one instance of a benchmark
# in our benchmark reporting scheme.  Arguments:
# 1 - benchmark name
# 2 - size (of benchmark data)
# 3 - configuration name

#Note: These are used only by the analysis mode, but 
# there's no harm in having them for all modes.
export gpugc_dropoff_chart_file=dropoff_$1_$2.dat
export gpugc_refdist_chart_file=refdist_$1_$2.dat
export gpugc_stats_file=stats_$1_$2.dat

# The dacapo benchmarks are nicely integrated with jikes' reporting mechanism
# for overall numbers, use this.  (-C triggers convergence)
export LD_LIBRARY_PATH=./tools/bootImageRunner/opencl/:$LD_LIBRARY_PATH
./dist/$3/rvm -X:gc:noReferenceTypes=true -X:gc:verboseTiming=true -jar ~/dacapo-9.12-bach.jar $1 --size $2 -c MMTkCallback -C

#This version gets the GC heap sizes.
#./dist/$3/rvm -X:gc:noReferenceTypes=true -verbose:gc -jar ~/dacapo-9.12-bach.jar $1 --size $2 -c MMTkCallback -C

# jikes provides a means to get per-GC timing information, but this is less reliable.  Use at your own risk.  (Incompatable with the above.)
#./dist/BaseBaseGPU_x86_64-linux/rvm -X:gc:noReferenceTypes=true -X:gc:verboseTiming=true -verbose:gc -jar ~/dacapo-9.12-bach.jar luindex
