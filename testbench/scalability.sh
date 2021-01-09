#!/usr/bin/env bash
SOCKET_NO=0

# Insert 120 milliion and, each thread read 10 million

for t in 2 4 8 16 32 60
do
    numactl -N $SOCKET_NO sudo ../release/hash_bench  --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=10000000 --read=10000000 --num=120000000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread.turbo_$t

    numactl -N $SOCKET_NO sudo ../release/hash_bench_30  --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=10000000 --read=10000000 --num=120000000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread.turbo30_$t

    numactl -N $SOCKET_NO sudo ../CCEH-PMDK/ycsb_bench --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=10000000 --read=10000000 --num=120000000 | tee thread.cceh_$t

    numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=10000000 --read=10000000 --num=120000000 | tee thread.clht30_$t
done



