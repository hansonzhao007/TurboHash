#!/bin/bash

rm *.parse

# 
csplit -k gc.data -f load --elide-empty-files '/Start IPMWatcher for delete/1' '{*}'
csplit -k load01 -f deleteoverwrite --elide-empty-files '/Start IPMWatcher for gc/1' '{*}'

mv load00 load.parse
mv deleteoverwrite00 deleteoverwrite.parse
mv deleteoverwrite01 gc.parse

# Parse the latency frequency
for i in load deleteoverwrite gc
do
    index=0
    datafile="${i}.parse"
    outfile="readlat_${i}_$index.parse"
    while read line; do
        if [ -n "$(echo $line | grep "Nanoseconds per op")" ]; then          
            index=$((index + 1))
            echo $index
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
for i in load deleteoverwrite gc
do
    index=0
    datafile="${i}.parse"
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

python3 plot.py

rm *.parse