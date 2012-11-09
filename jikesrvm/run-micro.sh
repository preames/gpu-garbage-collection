
#TODO: Instrument with analyis flags.
export LD_LIBRARY_PATH=./tools/bootImageRunner/opencl/:$LD_LIBRARY_PATH

./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/BigGCTest BigGCTest
./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/basic-cg GCTest
./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/classes Empty
./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/classes LinkedList
./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/classes LinkedListX256
./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/classes BigArray
./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/classes ArrayList2
./dist/$1/rvm -X:gc:noReferenceTypes=true -X:gc:harnessAll=true -cp ../benchmarks/micro/classes Turnover
