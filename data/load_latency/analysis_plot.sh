#!/bin/bash


rm *.parse

# Parse the latency frequency
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    datafile="load_latency.${i}"
    outfile="loadlat_${i}.parse"

    cat ${datafile} | grep -i -A 30 "loadlat      :" > "tmp.log"
    # outfile=load_$datafile.parse
    while read line; do
        if [[ $line = \(* ]]; then 
            tmp=`echo $line | awk '{print $3 "," $5}'`
            echo $tmp >> $outfile
            fi
    done < "tmp.log"
done


# Parse the latency statistics
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    index=0
    datafile="load_latency.${i}"
    outfile="loadlat_${i}_$index.parse"
    while read line; do
        if [ -n "$(echo $line | grep "Nanoseconds per op")" ]; then          
            index=$((index + 1))
            if [[ $index == 1 ]]; then
                outfile="loadlat_${i}_exist_stat.parse"
            else
                outfile="loadlat_${i}_non_stat.parse"
            fi
            echo "avg,std,min,median,max,p50,p75,p99,p999,p9999," >> $outfile
        fi

        if [ -n "$(echo $line | grep "Average: ")" ]; then
            avg_std=`echo $line | awk '{print $4, ",", $6}'`  
            printf "$avg_std," >> $outfile                  
        fi

        if [ -n "$(echo $line | grep "Min: ")" ]; then
            min_median_max=`echo $line | awk '{print $2, ",", $4, ",", $6}'`  
            printf "$min_median_max," >> $outfile                  
        fi

        if [ -n "$(echo $line | grep "Percentiles: ")" ]; then
            per=`echo $line |  awk '{print $3,",",$5,",",$7,",",$9,",",$11}'`  
            printf "$per," >> $outfile                  
        fi

    done < $datafile
done


# Parse the io
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    
    datafile="load_latency.${i}"
    outfile="loadio_${i}.parse"    
  
    echo "loadlat_r,loadlat_w" >> $outfile
    
    read_io=`grep "DIMM-R" $datafile | awk '{print $4}'`
    write_io=`grep "DIMM-W" $datafile | awk '{print $4}'`   
    echo "$read_io,$write_io" >> $outfile
done


python3 plot.py