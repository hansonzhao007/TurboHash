NUM=100000000
BUCKET=131072
CELL=64

for d in 0 1
do

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.1 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_10per.data

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.2 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_20per.data

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.3 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_30per.data

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.4 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_40per.data

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.5 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_50per.data

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.6 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_60per.data

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.7 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_70per.data

numactl -N 0 sudo ../release/hash_bench --bucket_count=$BUCKET --cell_count=$CELL --thread=40 --benchmarks=load,deleterepeat --stats_interval=100000000 --num=$NUM --read=0 --loadfactor=0.8 --no_rehash=true --repeat_delete=$d > delete_repeat_${d}_80per.data

done