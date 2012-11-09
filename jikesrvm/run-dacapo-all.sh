# This script is used internally to the results generation.  If you call this
# manually, make sure you check your results carefully.  
# argument - configuration name (such as FastAdaptiveMarkSweep_x86_64-linux)
#
# Note: Does not enable or disable gpugc environment variables.  You
# must do that before calling this. (except the file name variables,
# those generate per benchmark)
#
# Note: Several of the dacapo benchmarks are known not to work with
# the base version of JikesRVM used.  To see if something should work
# try running with BaseBaseMarkSweep before running any GPU versions.

#./run-dacapo-one.sh avrora small $1 #crashes (in analysis)
#./run-dacapo-one.sh batik small $1 #crashes
#./run-dacapo-one.sh eclipse small $1 #crashes
#./run-dacapo-one.sh fop small $1 #crashes
#./run-dacapo-one.sh h2 small $1 #crashes
#./run-dacapo-one.sh jython small $1 #crashes
#./run-dacapo-one.sh luindex small $1 # runs
#./run-dacapo-one.sh lusearch small $1 # runs
#./run-dacapo-one.sh pmd small $1 # runs
#./run-dacapo-one.sh sunflow small $1 #runs
#./run-dacapo-one.sh tomcat small $1 #crash
#./run-dacapo-one.sh tradebeans small $1 #crash
#./run-dacapo-one.sh tradesoap small $1 #crash
#./run-dacapo-one.sh xalan small $1 #runs

./run-dacapo-one.sh avrora default $1
#./run-dacapo-one.sh batik default  $1
#./run-dacapo-one.sh eclipse default  $1
#./run-dacapo-one.sh fop default  $1
#./run-dacapo-one.sh h2 default  $1
#./run-dacapo-one.sh jython default $1 
./run-dacapo-one.sh luindex default  $1
./run-dacapo-one.sh lusearch default  $1
./run-dacapo-one.sh pmd default  $1
./run-dacapo-one.sh sunflow default $1 
#./run-dacapo-one.sh tomcat default  $1
#./run-dacapo-one.sh tradebeans default $1 
#./run-dacapo-one.sh tradesoap default  $1
./run-dacapo-one.sh xalan default  $1
