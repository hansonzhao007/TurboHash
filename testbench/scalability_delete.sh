#!/usr/bin/env bash
SOCKET_NO=0

# Insert 120 milliion and, each thread read 10 million

for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # numactl -N $SOCKET_NO sudo ../Dash/release/ycsb_bench  --thread=$t --benchmarks=load,delete  --stats_interval=10000000 --read=10000000 --num=120000000 | tee thread_delete.dash_$t

    numactl -N $SOCKET_NO sudo ../release/hash_bench  --thread=$t --benchmarks=load,delete --stats_interval=10000000 --read=10000000 --num=120000000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread_delete.turbo_$t

    numactl -N $SOCKET_NO sudo ../release/hash_bench_30  --thread=$t --benchmarks=load,delete  --stats_interval=10000000 --read=10000000 --num=120000000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee thread_delete.turbo30_$t

    # numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clht30 --thread=$t --benchmarks=load,delete  --stats_interval=10000000 --read=10000000 --num=120000000 | tee thread_delete.clht30_$t

    # numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clevel30 --thread=$t --benchmarks=load,delete  --stats_interval=10000000 --read=10000000 --num=120000000 | tee thread_delete.clevel30_$t
done



