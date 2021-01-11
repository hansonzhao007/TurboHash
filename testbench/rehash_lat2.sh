#!/usr/bin/env bash

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=131072 --cell_count=16 --thread=8 --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_16

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=65536 --cell_count=32 --thread=8 --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_32

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=32768 --cell_count=64 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_64

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=16384 --cell_count=128 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_128

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=8192 --cell_count=256 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_256

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=4096 --cell_count=512 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_512

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=2048 --cell_count=1024 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_1024

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=1024 --cell_count=2048 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_2048

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=512 --cell_count=4096 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_4096

numactl -N 0 sudo ../release/hash_bench_30 --bucket_count=256 --cell_count=8192 --thread=8  --benchmarks=load,readrandom,rehashlat --stats_interval=100000000 --num=20000000 --no_rehash=true --read=0 --loadfactor=0.74 | tee  rehashlat2_8192
