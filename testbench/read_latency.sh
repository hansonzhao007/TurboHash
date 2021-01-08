#!/usr/bin/env bash
SOCKET_NO=0

# Insert 100 milliion and, each thread read 10 million
numactl -N $SOCKET_NO sudo ../release/hash_bench  --thread=16 --benchmarks=load,readrandom,readlat,readnotlat --stats_interval=10000000 --read=10000000 --num=100000000 --no_rehash=false | tee read_latency.turbo

