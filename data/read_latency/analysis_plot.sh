#!/bin/bash


rm *.parse

# Parse the latency frequency
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
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
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
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
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    
    datafile="read_latency.${i}"
    outfile="readio_${i}.parse"    
    
    echo "readlat_r,readlat_w,readnonlat_r,readnonlat_w" >> $outfile

    # all data
    raw=`grep "DIMM-R:" $datafile | awk '{print$2}'`        
    arr=(${raw//:/ })
    
    # readlat_r
    read_r="${arr[2]}"
    # readnonlat_r
    readnon_r="${arr[3]}"  

    raw=`grep "DIMM-W:" $datafile | awk '{print$2}'`        
    arr=(${raw//:/ })

    # readlat_w
    read_w="${arr[2]}"
    # readnonlat_w
    readnon_w="${arr[3]}"  

    echo "$read_r,$read_w,$readnon_r,$readnon_w" >> $outfile
done


python3 plot.py