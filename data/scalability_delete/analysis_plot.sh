#!/bin/bash


rm *.parse

outfile_delete="scalability_delete.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete

outfile_delete_io_r="scalability_delete_io_r.parse"
outfile_delete_io_w="scalability_delete_io_w.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_io_r
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_io_w

outfile_delete_bw_r="scalability_delete_bw_r.parse"
outfile_delete_bw_w="scalability_delete_bw_w.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_bw_r
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_bw_w

for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # parse the throughput
    oneline_delete="$t"
    for i in clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread_delete.${i}_${t}"

        tmp=`cat $datafile | grep "delete       :" | awk '{print$5}'` 
        oneline_delete="$oneline_delete,$tmp"
    done
    echo $oneline_delete >> $outfile_delete

    # parse the io read
    oneline_delete="$t"         
    for i in clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread_delete.${i}_${t}"
        # all data
        raw=`grep "DIMM-R:" $datafile | awk '{print$4}'`        
        arr=(${raw//:/ })
 
        oneline_delete="$oneline_delete,${arr[1]}"  
    done
    echo $oneline_delete >> $outfile_delete_io_r

    # parse the io read
    oneline_delete="$t"         
    for i in clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread_delete.${i}_${t}"
        # all data
        raw=`grep "DIMM-W:" $datafile | awk '{print$4}'`        
        arr=(${raw//:/ })
 
        oneline_delete="$oneline_delete,${arr[1]}"  
    done
    echo $oneline_delete >> $outfile_delete_io_w

    # Parse the bw read
    oneline_delete="$t"   
    for i in clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread_delete.${i}_${t}"
        # all data
        raw=`grep "DIMM-R:" $datafile | awk '{print$2}'`        
        arr=(${raw//:/ })
        
        oneline_delete="$oneline_delete,${arr[1]}"         
    done   
    echo $oneline_delete >> $outfile_delete_bw_r

    # Parse the bw write
    oneline_delete="$t"   
    for i in clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread_delete.${i}_${t}"
        # all data
        raw=`grep "DIMM-W:" $datafile | awk '{print$2}'`        
        arr=(${raw//:/ })
        
        oneline_delete="$oneline_delete,${arr[1]}"         
    done   
    echo $oneline_delete >> $outfile_delete_bw_w

done

python3 plot.py