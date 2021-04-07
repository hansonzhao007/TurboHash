
THREAD_NUM=4
NUM=20000000

# for b in 2048 4096 8192 16384 32768 65536 131072 262144 524288
for b in 4096 16384 65536
do
numactl -N 0 sudo ../release/hash_bench  --thread=$THREAD_NUM --benchmarks=load,readrandom --stats_interval=10000000 --num=$NUM --bucket_count=$b --cell_count=16 --no_rehash=false --use_existing_db=false
sleep 1
numactl -N 0 sudo ../release/hash_bench  --thread=$THREAD_NUM --benchmarks=readrandom      --stats_interval=10000000 --num=$NUM --use_existing_db=true | tee recover_$b.log
done


# ../release/hash_bench --thread=4 --benchmarks=load,readrandom --stats_interval=10000000 --num=20000000 --bucket_count=4096 --cell_count=16 --no_rehash=false --use_existing_db=false

# ../release/hash_bench --thread=4 --benchmarks=readrandom --stats_interval=10000000 --num=10000000 --bucket_count=2048 --cell_count=16 --no_rehash=false --use_existing_db=true