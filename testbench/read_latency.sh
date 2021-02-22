#!/usr/bin/env bash
SOCKET_NO=0

# Insert 100 milliion and, each thread read 10 million

numactl -N $SOCKET_NO sudo ../release/hash_bench  --thread=16 --benchmarks=load,readrandom,readlat,readnonlat --stats_interval=10000000 --read=10000000 --num=100000000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee read_latency.turbo

numactl -N $SOCKET_NO sudo ../release/hash_bench_30  --thread=16 --benchmarks=load,readrandom,readlat,readnonlat --stats_interval=10000000 --read=10000000 --num=100000000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee read_latency.turbo30

# numactl -N $SOCKET_NO sudo ../CCEH-PMDK/ycsb_bench --thread=16 --benchmarks=load,readrandom,readlat,readnonlat --stats_interval=10000000 --read=10000000 --num=100000000 | tee read_latency.cceh


# numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench --thread=16 --benchmarks=load,readrandom,readlat,readnonlat --stats_interval=10000000 --read=10000000 --num=100000000 | tee read_latency.clevel30



