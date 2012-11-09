# Used for the baseline comparison with JIKES
ant -Dconfig.name=FastAdaptiveMarkSweep
# Used for the Mixed, and Analysis (Do we need a GPUOnly mode?)
ant -Dconfig.name=FastAdaptiveGPU
echo "Done!"
