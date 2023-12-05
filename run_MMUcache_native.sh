#!/bin/bash
export LD_LIBRARY_PATH="/home/msrc/benchmarks/tools/papi-5.2.0/src/instdir/lib64/:$LD_LIBRARY_PATH"

conf=MMUcache_native
rm -r results_${conf}
mkdir -p results_${conf}
pageStride=1
#for pageStride in 512 4096 8192 32768 65536 131072 262144 524288; do
for pageStride in 512 262144; do
    #for numAccesses in 4 5 8 9 16 32 64 128 256; do
	for numAccesses in 4 5 6 7 8 9 10 16 32 64 128 256; do
		echo "running for pageStride:$pageStride and numAccesses:$numAccesses"
		for (( times=0; times<100; times++ )); do
			taskset -c 2 \
			./babka -p 3145728 -s $pageStride -a $numAccesses -o 0 > \
			results_${conf}/output_${pageStride}_${numAccesses}_${times}.txt
		done
	done
done
