numactl -N 0 ../release/hash_bench_old --thread_write=1 --thread_read=4
# numactl -N 0 ../release/hash_bench  --thread=8 --benchmarks=rehashspeed --bucket_count=131072 --cell_count=32
# numactl -N 0 ../release/hash_bench  --thread=8 --benchmarks=load,readrandom,overwrite,readrandom,proberandom,rehash --stats_interval=50000000