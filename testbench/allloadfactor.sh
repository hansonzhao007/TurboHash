#!/usr/bin/env bash
SOCKET_NO=0
# numactl -N $SOCKET_NO ../release/hash_bench  --thread=1 --benchmarks=allloadfactor,readrandom --stats_interval=10000000 --num=100000000 --no_rehash=false --bucket_count=65536 --cell_count=16 | tee all_loadfactor.turbo

# numactl -N $SOCKET_NO sudo ../CCEH-PMDK/ycsb_bench  --thread=1 --benchmarks=allloadfactor,readrandom --stats_interval=10000000 --num=100000000 read=10000000 --initsize=16 | tee all_loadfactor.cceh

numactl -N $SOCKET_NO sudo ../Clevel-Hashing/release/tests/ycsb_bench  --thread=1 --benchmarks=allloadfactor,readrandom --stats_interval=10000000 read=10000000 --batch=10000 --num=16000000 | tee all_loadfactor.clevel30

