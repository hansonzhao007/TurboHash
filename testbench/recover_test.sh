
THREAD_NUM=16
NUM=10000000
numactl -N 0 sudo ./hash_bench  --thread=$THREAD_NUM --benchmarks=load,readrandom --stats_interval=10000000 --num=$NUM --no_rehash=false --use_existing_db=false
numactl -N 0 sudo ./hash_bench  --thread=$THREAD_NUM --benchmarks=readrandom      --stats_interval=10000000 --num=$NUM --use_existing_db=true