#!/bin/bash

rm *.parse

# Parse the io
for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
do
    datafile="thread.${i}_16"
    outfile="io_${i}.parse"    
    read_io=0.0
    write_io=0.0

    index=0
    echo "load_r,load_w,read_r,read_w,readnon_r,readnon_w" >> $outfile
    while read line; do
        if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
            if [[ $read_io != 0.0 ]]; then                
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