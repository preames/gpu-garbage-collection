rm -f microbench*.dat
export gpugc_checked=false

# CPU only
export gpugc_mode=cpu_only
./gpugc
mkdir -p data/cpu_only
mv microbench*.dat data/cpu_only

# GPU Only, Histogram, no-vector, no-fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=false
export gpugc_use_divergence=false
./gpugc
mkdir -p data/gpu-hist-novec-nofall
mv microbench*.dat data/gpu-hist-novec-nofall/

# GPU Only, Prefix Sum, no-vector, no-fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=true
export gpugc_use_vectors=false
export gpugc_use_divergence=false
./gpugc
mkdir -p data/gpu-prefix-novec-nofall
mv microbench*.dat data/gpu-prefix-novec-nofall/

# GPU Only, Histogram, vector, no-fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=true
export gpugc_use_divergence=false
./gpugc
mkdir -p data/gpu-hist-vec-nofall
mv microbench*.dat data/gpu-hist-vec-nofall/

# GPU Only, Histogram, no-vector, fallback
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=20
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=false
export gpugc_use_divergence=false
./gpugc
mkdir -p data/gpu-hist-novec-fall20
mv microbench*.dat data/gpu-hist-novec-fall20/

# GPU Only, histogram, no-vector, no-fallback, divergence
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=false
export gpugc_use_divergence=true
./gpugc
mkdir -p data/gpu-hist-novec-nofall-diverg
mv microbench*.dat data/gpu-hist-novec-nofall-diverg/


# GPU Only, histogram, vector, no fallback, divergence
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=true
export gpugc_use_divergence=true
export gpugc_use_both_compute=false
export gpugc_use_load_balence=false
./gpugc
mkdir -p data/gpu-hist-vec-nofall-diverg
mv microbench*.dat data/gpu-hist-vec-nofall-diverg/

# GPU Only, histogram, vector, no fallback, divergence, two compute
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=true
export gpugc_use_divergence=true
export gpugc_use_both_compute=true
export gpugc_use_load_balence=false
./gpugc
mkdir -p data/gpu-hist-vec-nofall-diverg-both
mv microbench*.dat data/gpu-hist-vec-nofall-diverg-both/


exit
# GPU Only, histogram, vector, no fallback, divergence, two compute
export gpugc_mode=gpu_only
export gpugc_cpu_fallback=0
export gpugc_use_prefix_sum=false
export gpugc_use_vectors=true
export gpugc_use_divergence=true
export gpugc_use_both_compute=true
#export gpugc_use_load_balence=true
#./gpugc
#mkdir -p data/gpu-hist-vec-nofall-diverg-both-lb
#mv microbench*.dat data/gpu-hist-vec-nofall-diverg-both-lb/
