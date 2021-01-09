#!/bin/bash


rm *.parse

# Parse the latency frequency
for i in cceh cceh30 clevel30 clht30 turbo turbo30
do
    index=0
    datafile="read_latency.${i}"
    outfile="readlat_${i}_$index.parse"
    while read line; do
        if [ -n "$(echo $line | grep "Nanoseconds per op")" ]; then          
            index=$((index + 1))
            if [[ $index == 1 ]]; then
                outfile="readlat_${i}_exist.parse"
            else
                outfile="readlat_${i}_non.parse"
            fi
            echo "ns,frequency" >> $outfile
        fi

        if [[ $line = \(* ]]; then 
            tmp=`echo $line | awk '{print $3 "," $5}'`
            echo $tmp >> $outfile
        fi
    done < $datafile
done


# Parse the latency statistics
for i in cceh cceh30 clevel30 clht30 turbo turbo30
do
    index=0
    datafile="read_latency.${i}"
    outfile="readlat_${i}_$index.parse"
    while read line; do
        if [ -n "$(echo $line | grep "Nanoseconds per op")" ]; then          
            index=$((index + 1))
            if [[ $index == 1 ]]; then
                outfile="readlat_${i}_exist_stat.parse"
            else
                outfile="readlat_${i}_non_stat.parse"
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


# Parse the read io
for i in cceh cceh30 clevel30 clht30 turbo turbo30
do
    
    datafile="read_latency.${i}"
    outfile="readio_${i}.parse"    
    read_io=0.0
    write_io=0.0

    index=0
    echo "readlat_r,readlat_w,readnonlat_r,readnonlat_w" >> $outfile
    while read line; do
        if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
            if [[ $index > 2 ]]; then                
                printf "$read_io,$write_io," >> $outfile
            fi
            read_io=0.0
            write_io=0.0
            index=$((index + 1))
        fi

        if [ -n "$(echo $line | grep "MB |")" ]; then  
            tmp_read=`echo $line | awk '{print $7}'`
            tmp_write=`echo $line | awk '{print $9}'`
            read_io=`echo $read_io + $tmp_read | bc`
            write_io=`echo $write_io + $tmp_write | bc`
        fi
    done < $datafile
    echo "$read_io,$write_io" >> $outfile
done


python3 plot.py