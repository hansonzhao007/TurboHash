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

    tmp=`grep "load " $datafile | awk '{print $5}'`
    oneline="$oneline,$tmp"

    tmp=`grep "ycsbd " $datafile | awk '{print $5}'`
    oneline="$oneline,$tmp"

    tmp=`grep "ycsba " $datafile | awk '{print $5}'`
    oneline="$oneline,$tmp"

    tmp=`grep "ycsbb " $datafile | awk '{print $5}'`
    oneline="$oneline,$tmp"

    tmp=`grep "ycsbc " $datafile | awk '{print $5}'`
    oneline="$oneline,$tmp"

    tmp=`grep "ycsbf " $datafile | awk '{print $5}'`
    oneline="$oneline,$tmp"

    echo "$oneline" >> $outfile
done


python3 plot.py