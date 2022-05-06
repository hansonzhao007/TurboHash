#!/usr/bin/env bash
SOCKET_NO=0
NUM=120960000
# Insert 120 milliion and, each thread read 10 million

for t in 40 36 32 28 24 20 16 8 4 2 1
# for t in 1 2 4 8 16 20 24 28 32 36 40
do
    numactl -N $SOCKET_NO sudo ../Dash/release/ycsb_bench  --thread=$t --benchmarks=load,delete,readrandom  --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread_delete.dash_$t

    numactl -N $SOCKET_NO sudo ../release/hash_bench_pmdk  --thread=$t --benchmarks=load,delete,readrandom --stats_interval=200000000 --read=10000000 --num=${NUM} --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread_delete.turbo_$t

    numactl -N $SOCKET_NO sudo ../release/hash_bench_pmdk_30  --thread=$t --benchmarks=load,delete,readrandom  --stats_interval=200000000 --read=10000000 --num=${NUM} --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread_delete.turbo30_$t

    numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clht30 --thread=$t --benchmarks=load,delete,readrandom  --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread_delete.clht30_$t

    numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clevel30 --thread=$t --benchmarks=load,delete,readrandom  --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread_delete.clevel30_$t
done


