#!/bin/bash


rm *.parse

outfile_update="scalability_update.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update

outfile_update_io_r="scalability_update_io_r.parse"
outfile_update_io_w="scalability_update_io_w.parse"

echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_io_r
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_io_w


outfile_update_bw_r="scalability_update_bw_r.parse"
outfile_update_bw_w="scalability_update_bw_w.parse"

echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_bw_r
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_bw_w


for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # parse the throughput
    oneline_update="$t"
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"

        tmp=`cat $datafile | grep "overwrite " | awk '{print$5}'` 
        oneline_update="$oneline_update,$tmp"
    done
    echo $oneline_update >> $outfile_update

    # parse the io read
    oneline_update="$t"         
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"
        # all data
        raw=`grep "DIMM-R:" $datafile | awk '{print$4}'`        
        arr=(${raw//:/ })
 
        oneline_update="$oneline_update,${arr[1]}"  
    done
    echo $oneline_update >> $outfile_update_io_r

    # parse the io read
    oneline_update="$t"         
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"
        # all data
        raw=`grep "DIMM-W:" $datafile | awk '{print$4}'`        
        arr=(${raw//:/ })
 
        oneline_update="$oneline_update,${arr[1]}"  
    done
    echo $oneline_update >> $outfile_update_io_w

    # Parse the bw read
    oneline_update="$t"   
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"
        # all data
        raw=`grep "DIMM-R:" $datafile | awk '{print$2}'`        
        arr=(${raw//:/ })
        
        oneline_update="$oneline_update,${arr[1]}"         
    done   
    echo $oneline_update >> $outfile_update_bw_r

    # Parse the bw write
    oneline_update="$t"   
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"
        # all data
        raw=`grep "DIMM-W:" $datafile | awk '{print$2}'`        
        arr=(${raw//:/ })
        
        oneline_update="$oneline_update,${arr[1]}"         
    done   
    echo $oneline_update >> $outfile_update_bw_w
done

python3 plot.py