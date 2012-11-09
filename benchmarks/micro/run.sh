#Note: This file is for building, and sanity checking the benchmarks.
# It plays no role in the benchmark and does not use Jikes at all!  

echo "building"
mkdir -p classes
javac -d classes *.java
mkdir -p results
echo "Running benchmarks"
java -cp classes Empty
java -cp classes LinkedList
java -cp classes LinkedListX256
java -cp classes BigArray
java -cp classes ArrayList2
java -cp classes Turnover
