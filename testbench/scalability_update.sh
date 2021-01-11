#!/usr/bin/env bash
SOCKET_NO=0

# Insert 120 milliion and, each thread read 10 million

for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # numactl -N $SOCKET_NO sudo ../CCEH-PMDK/ycsb_bench --thread=$t --benchmarks=load,overwrite,readrandom --stats_interval=10000000 --read=10000000 --num=120000000 | tee thread_update.cceh_$t

    numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench --thread=$t --benchmarks=load,overwrite,readrandom  --stats_interval=10000000 --read=10000000 --num=120000000 | tee thread_update.cceh30_$t
done



