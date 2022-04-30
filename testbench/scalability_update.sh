#!/usr/bin/env bash
SOCKET_NO=0
NUM=120960000
# Insert 120 milliion and, each thread read 10 million

for t in 1 2 4 8 16 20 24 28 32 36 40
do
    numactl -N $SOCKET_NO sudo ../Dash/release/ycsb_bench  --thread=$t --benchmarks=load,overwrite,readrandom  --stats_interval=10000000 --read=10000000 --num=${NUM} | tee thread_update.dash_$t

    numactl -N $SOCKET_NO sudo ../release/hash_bench  --thread=$t --benchmarks=load,overwrite,readrandom --stats_interval=10000000 --read=10000000 --num=${NUM} --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread_update.turbo_$t

    numactl -N $SOCKET_NO sudo ../release/hash_bench_30  --thread=$t --benchmarks=load,overwrite,readrandom  --stats_interval=10000000 --read=10000000 --num=${NUM} --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread_update.turbo30_$t
    
    numactl -N $SOCKET_NO sudo ../CCEH-PMDK/ycsb_bench --thread=$t --benchmarks=load,overwrite,readrandom --stats_interval=10000000 --read=10000000 --num=${NUM} | tee thread_update.cceh_$t

    numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_cceh30 --thread=$t --benchmarks=load,overwrite,readrandom  --stats_interval=10000000 --read=10000000 --num=${NUM} | tee thread_update.cceh30_$t

    numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clevel30 --thread=$t --benchmarks=load,overwrite,readrandom  --stats_interval=10000000 --read=10000000 --num=${NUM} | tee thread_update.clevel30_$t

    numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clht30 --thread=$t --benchmarks=load,overwrite,readrandom  --stats_interval=10000000 --read=10000000 --num=${NUM} | tee thread_update.clht30_$t

done



