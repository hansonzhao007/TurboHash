#!/usr/bin/env bash
SOCKET_NO=0
numactl -N $SOCKET_NO sudo ./hash_bench  --thread=8 --benchmarks=load,readrandom --stats_interval=10000000 --read=500000000 --num=1000000000 --no_rehash=false --report_interval=1 | tee load.data
