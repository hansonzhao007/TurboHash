#!/usr/bin/env bash

for CELL in 16 32 64 128 256 512 1024 2048 4096 8192
do
    b_count=$((2097152 / CELL))
    echo $b_count
    numactl -N 0 sudo ../release/hash_bench_pmdk    --bucket_count=$b_count --cell_count=$CELL --thread=8 --benchmarks=load,readrandom,rehashlat --stats_interval=2000000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat$CELL

    numactl -N 0 sudo ../release/hash_bench_pmdk_30 --bucket_count=$b_count --cell_count=$CELL --thread=8 --benchmarks=load,readrandom,rehashlat --stats_interval=2000000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_$CELL
done