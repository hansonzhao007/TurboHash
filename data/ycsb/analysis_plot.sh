#!/bin/bash

rm *.parse

# Parse the throughput
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    datafile="ycsb.${i}"
    outfile="ycsb_${i}.parse"
    oneline=""
    index=0
    echo "load,ycsbd,ycsba,ycsbb,ycsbc,ycsbf" >> $outfile
    while read line; do
        if [ -n "$(echo $line | grep "load ")" ]; then  
            iops=`echo $line |  awk '{print $5}'`
            oneline="$iops"
        fi

        if [ -n "$(echo $line | grep "ycsba ")" ]; then  
            iops=`echo $line |  awk '{print $5}'`
            oneline="$oneline,$iops"
        fi

        if [ -n "$(echo $line | grep "ycsbb ")" ]; then  
            iops=`echo $line |  awk '{print $5}'`
            oneline="$oneline,$iops"
        fi

        if [ -n "$(echo $line | grep "ycsbc ")" ]; then  
            iops=`echo $line |  awk '{print $5}'`
            oneline="$oneline,$iops"
        fi

        if [ -n "$(echo $line | grep "ycsbd ")" ]; then  
            iops=`echo $line |  awk '{print $5}'`
            oneline="$oneline,$iops"
        fi

        if [ -n "$(echo $line | grep "ycsbf ")" ]; then  
            iops=`echo $line |  awk '{print $5}'`
            oneline="$oneline,$iops"
        fi
    done < $datafile
    echo "$oneline" >> $outfile
done


python3 plot.py