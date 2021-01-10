#!/bin/bash


rm *.parse

# Parse throughput
outfile_load="scalability_load.parse"
outfile_update="scalability_update.parse"
outfile_delete="scalability_delete.parse"
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_read
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_readnon
for t in 1 2 4 8 16 20 24 28 32 36 40
do
    oneline_load="$t"
    oneline_update="$t"
    oneline_delete="$t"
    for i in clevel30 clht30 turbo turbo30
    do
        datafile="thread_update_delete.${i}_${t}"
        while read line; do
            if [ -n "$(echo $line | grep "load ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_load="$oneline_load,$tmp"
            fi

            if [ -n "$(echo $line | grep "overwrite ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_update="$oneline_update,$tmp"
            fi

            if [ -n "$(echo $line | grep "delete ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_delete="$oneline_delete,$tmp"
            fi
        done < $datafile
    done
    echo $oneline_load >> $outfile_load
    echo $oneline_update >> $outfile_update
    echo $oneline_delete >> $outfile_delete
done

# python3 plot.py