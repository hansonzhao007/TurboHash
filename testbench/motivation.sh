rm perf.data.*
evens=L1-dcache-loads:u,L1-dcache-load-misses:u,dTLB-loads:u,dTLB-load-misses:u

filename=motivation1.csv
rm $filename
echo "access_size, rand_read_latency, seq_read_latency, rand_write_latency, seq_write_latency, \
      rnd_r_l1_load, rnd_r_l1_miss, rnd_r_tlb_load, rnd_r_tlb_miss, \
      seq_r_l1_load, seq_r_l1_miss, seq_r_tlb_load, seq_r_tlb_miss, \
      rnd_w_l1_load, rnd_w_l1_miss, rnd_w_tlb_load, rnd_w_tlb_miss, \
      seq_w_l1_load, seq_w_l1_miss, seq_w_tlb_load, seq_w_tlb_miss, " > $filename

for size in 1 2 4 8 16 32 ;
do
    echo " ========= $size ======= "
    rm perf.data.*
    perf record -e $evens --switch-out -- numactl -N 0 ../release/motivation  --filename=$filename --loop=$size
    i=0
    for file in perf.data.*;
    do
        if [ $i == 0 ]
        then
            echo jump $file
        else
            echo "---------- size $size result ----------"
            echo -n "      " >> $filename
            perf report -i $file --stdio | tee tmp.txt | grep "Event count" | awk 'BEGIN { ORS=", " }; { print $5 }      ' >> $filename
        fi
        i=$((i + 1))
    done
    echo "      " >> $filename
done
