#!/bin/bash


rm *.parse

# Parse throughput
outfile_load="scalability_load.parse"
outfile_read="scalability_read.parse"
outfile_readnon="scalability_readnon.parse"
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30" >> $outfile_load
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30" >> $outfile_read
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30" >> $outfile_readnon
for t in 1 2 4 8 16 20 24 28 32 36 40
do
    oneline_load="$t"
    oneline_read="$t"
    oneline_readnon="$t"
    for i in cceh cceh30 clevel30 clht30 turbo turbo30
    do
        datafile="thread.${i}_${t}"
        while read line; do
            if [ -n "$(echo $line | grep "load ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_load="$oneline_load,$tmp"
            fi

            if [ -n "$(echo $line | grep "readrandom ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_read="$oneline_read,$tmp"
            fi

            if [ -n "$(echo $line | grep "readnon ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_readnon="$oneline_readnon,$tmp"
            fi
        done < $datafile
    done
    echo $oneline_load >> $outfile_load
    echo $oneline_read >> $outfile_read
    echo $oneline_readnon >> $outfile_readnon
done

# python3 plot.py