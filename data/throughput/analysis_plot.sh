#!/bin/bash

rm *.parse

# Parse the io
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    datafile="thread.${i}_16"
    outfile="io_${i}.parse"    
    echo $outfile

    index=0
    echo "load_r,load_w,read_r,read_w,readnon_r,readnon_w" >> $outfile
    
    # all data
    raw=`grep "DIMM-R:" $datafile | awk '{print$4}'`        
    echo $raw
    arr=(${raw//:/ })    
    load_r="${arr[0]}"
    # readlat_r
    read_r="${arr[1]}"
    # readnonlat_r
    readnon_r="${arr[2]}"  

    # all data
    raw=`grep "DIMM-W:" $datafile | awk '{print$4}'`  
    echo $raw      
    arr=(${raw//:/ })
    load_w="${arr[0]}"
    # readlat_w
    read_w="${arr[1]}"
    # readnonlat_w
    readnon_w="${arr[2]}"  

    echo "$load_r,$load_w,$read_r,$read_w,$readnon_r,$readnon_w" >> $outfile
done


# Parse the throughput
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    datafile="thread.${i}_16"
    outfile="throughput_${i}.parse"    
    read_io=0.0
    write_io=0.0

    index=0
    echo "load,read,readnon" >> $outfile
    while read line; do
        if [ -n "$(echo $line | grep "load ")" ]; then  
            load=`echo $line |  awk '{print $5}'`
        fi

        if [ -n "$(echo $line | grep "readrandom ")" ]; then  
            read=`echo $line |  awk '{print $5}'`
        fi

        if [ -n "$(echo $line | grep "readnon ")" ]; then  
            readnon=`echo $line |  awk '{print $5}'`
        fi
    done < $datafile
    echo "$load,$read,$readnon" >> $outfile
done


python3 plot.py