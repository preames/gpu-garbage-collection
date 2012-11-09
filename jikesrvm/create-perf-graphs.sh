
# should be default, just making sure
export gpugc_checked=false
export gpugc_notrace_output=true


# Run the default MS for comparison
./run-dacaop-all.sh FastAdaptiveMarkSweep_x86_64-linux 2>&1 | tee log-perf-dacapo-ms.log

 CPU only
export gpugc_mode=cpu_only
./run-dacapo-all.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-dacapo-cpu.log
mkdir -p data/cpu_only
cp stats*.dat data/cpu_only

export gpugc_checked=true

# GPU Only, Histogram, no-vector, no-fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=false
./run-dacapo-all.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-dacapo-gpu-hist-novec-nofall.log
mkdir -p data/gpu-hist-novec-nofall
cp stats*.dat data/gpu-hist-novec-nofall/

# GPU Only, Prefix Sum, no-vector, no-fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=true
export gpugc_use_vectors=false
./run-dacapo-all.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-dacapo-gpu-prefix-novec-nofall.log
mkdir -p data/gpu-prefix-novec-nofall
cp stats*.dat data/gpu-prefix-novec-nofall/

# GPU Only, Histogram, vector, no-fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=true
./run-dacapo-all.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-dacapo-gpu-hist-vec-nofall.log
mkdir -p data/gpu-hist-vec-nofall
cp stats*.dat data/gpu-hist-vec-nofall/


# GPU Only, Histogram, no-vector, fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=20
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=false
./run-dacapo-all.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-dacapo-gpu-hist-novec-fall20.log
mkdir -p data/gpu-hist-novec-fall20
cp stats*.dat data/gpu-hist-novec-fall20/


# GPU Only, Histogram, no-vector, no-fallback, Dual Compute Units w/LB
# NOTE: Needs checkout of different branch!
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=false
./run-dacapo-all.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-dacapo-gpu-hist-novec-nofall-2way.log
mkdir -p data/gpu-hist-novec-nofall-2way/
cp stats*.dat data/gpu-hist-novec-nofall-2way/


# GPU Only, Histogram, vector, no-fallback, divergence
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=true
export gpugc_use_divergence=true
./run-dacapo-all.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-dacapo-gpu-hist-vec-nofall-diverg.log
mkdir -p data/gpu-hist-vec-nofall-diverg/
cp stats*.dat data/gpu-hist-vec-nofall-diverg/



exit;
# Run the micro benchmarks first (for sanity sake)
./run-micro.sh FastAdaptiveMarkSweep_x86_64-linux 2>&1 | tee log-perf-micro-ms.log
export gpugc_mode=cpu_only
./run-micro.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-micro-cpu.log
#Really, run the mixed mode
export gpugc_mode=gpu_only
./run-micro.sh FastAdaptiveGPU_x86_64-linux 2>&1 | tee log-perf-micro-mixed.log
