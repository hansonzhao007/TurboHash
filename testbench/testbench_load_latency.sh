#!/usr/bin/env bash
SOCKET_NO=0

# Insert 100 milliion and, each thread read 10 million

numactl -N $SOCKET_NO sudo ../Dash/release/ycsb_bench  --thread=16 --benchmarks=loadlat --stats_interval=200000000 --read=10000000 --num=120960000 | tee load_latency.dash

numactl -N $SOCKET_NO sudo ../release/hash_bench_pmdk  --thread=16 --benchmarks=loadlat --stats_interval=200000000 --read=10000000 --num=120960000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee load_latency.turbo

numactl -N $SOCKET_NO sudo ../release/hash_bench_pmdk_30  --thread=16 --benchmarks=loadlat --stats_interval=200000000 --read=10000000 --num=120960000 --bucket_count=65536 --cell_count=16 --no_rehash=false | tee load_latency.turbo30

numactl -N $SOCKET_NO sudo ../CCEH-PMDK/ycsb_bench --thread=16 --benchmarks=loadlat --stats_interval=200000000 --read=10000000 --num=120960000 | tee load_latency.cceh

numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_cceh30 --thread=16 --benchmarks=loadlat --stats_interval=200000000 --read=10000000 --num=120960000 | tee load_latency.cceh30

numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clevel30 --thread=16 --benchmarks=loadlat --stats_interval=200000000 --read=10000000 --num=120960000 | tee load_latency.clevel30

numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench_clht30 --thread=16 --benchmarks=loadlat --stats_interval=200000000 --read=10000000 --num=120960000 | tee load_latency.clht30





