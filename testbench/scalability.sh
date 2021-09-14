#!/usr/bin/env bash
SOCKET_NO=0
NUM=120000000
# Insert 120 milliion and, each thread read 10 million


# for t in 1 2 4 8 16 20 24 28 32 36 40
for t in 4 8 16
do
    numactl -N $SOCKET_NO sudo ../release/hash_bench  --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=200000000 --read=10000000 --num=${NUM} --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread.turbo_$t

    # numactl -N $SOCKET_NO sudo ../release/hash_bench_30  --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=200000000 --read=10000000 --num=${NUM} --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread.turbo30_$t

    # numactl -N $SOCKET_NO sudo ../CCEH-PMDK/ycsb_bench --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread.cceh_$t

    # numactl -N $SOCKET_NO sudo ../Dash/release/ycsb_bench --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread.dash_$t

    # numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clevel30 --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread.clevel30_$t

    # numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clht30 --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread.clht30_$t

    # numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_cceh30 --thread=$t --benchmarks=load,readrandom,readnon --stats_interval=200000000 --read=10000000 --num=${NUM} | tee thread.cceh30_$t

done



