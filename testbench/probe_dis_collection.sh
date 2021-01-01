numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.1 > probe_dis_10per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.2 > probe_dis_20per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.3 > probe_dis_30per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.4 > probe_dis_40per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.5 > probe_dis_50per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.6 > probe_dis_60per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.7 > probe_dis_70per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.8 > probe_dis_80per.data

numactl -N 0 ./hash_bench --bucket_count=131072 --cell_count=16 --thread=1 --batch=1 --benchmarks=load,readrandom,stats --stats_interval=100000000 --num=27262976 --read=0 --loadfactor=0.9 --no_rehash=false > probe_dis_90per.data