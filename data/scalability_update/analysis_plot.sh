#!/bin/bash


rm *.parse

outfile_update="scalability_update.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update

outfile_update_io="scalability_update_io.parse"
outfile_update_io_r="scalability_update_io_r.parse"
outfile_update_io_w="scalability_update_io_w.parse"

echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_io
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_io_r
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_io_w


outfile_update_bw="scalability_update_bw.parse"
outfile_update_bw_r="scalability_update_bw_r.parse"
outfile_update_bw_w="scalability_update_bw_w.parse"


echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_bw
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_bw_r
echo "thread,clevel30,clht30,turbo,turbo30,dash,cceh,cceh30" >> $outfile_update_bw_w


for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # parse the throughput
    oneline_update="$t"
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"
        while read line; do            
            if [ -n "$(echo $line | grep "overwrite ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_update="$oneline_update,$tmp"
            fi
        done < $datafile
    done
    echo $oneline_update >> $outfile_update

    # parse the io    
    oneline_update="$t"    
    oneline_update_r="$t"
    oneline_update_w="$t"        
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"
        update_io=0.0
        write_io=0.0
        total_io=0.0
        index=0
        while read line; do
            if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
                if [[ $index -eq 2 ]]; then
                    oneline_update_r="$oneline_update_r,$update_io"
                    oneline_update_w="$oneline_update_w,$write_io"
                    oneline_update="$oneline_update,$total_io"
                fi
                update_io=0.0
                write_io=0.0
                total_io=0.0
                index=$((index + 1))
            fi

            if [ -n "$(echo $line | grep "MB |")" ]; then  
                tmp_read=`echo $line | awk '{print $7}'`
                tmp_write=`echo $line | awk '{print $9}'`
                update_io=`echo $update_io + $tmp_read | bc`
                write_io=`echo $write_io + $tmp_write | bc`
                total_io=`echo $write_io + $update_io | bc`
            fi
        done < $datafile
    done
    echo $oneline_update >> $outfile_update_io
    echo $oneline_update_r >> $outfile_update_io_r
    echo $oneline_update_w >> $outfile_update_io_w

    # Parse the bw
    oneline_update="$t"
    oneline_update_r="$t"
    oneline_update_w="$t"
    for i in clevel30 clht30 turbo turbo30 dash cceh cceh30
    do
        datafile="thread_update.${i}_${t}"
        update_bw=0.0
        write_bw=0.0
        total_bw=0.0
        index=0
        while read line; do
            if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
                if [[ $index -eq 2 ]]; then
                    oneline_update_r="$oneline_update_r,$update_bw"
                    oneline_update_w="$oneline_update_w,$write_bw"
                    oneline_update="$oneline_update,$total_bw"                
                fi
                update_bw=0.0
                write_bw=0.0
                total_bw=0.0
                index=$((index + 1))
            fi

            if [ -n "$(echo $line | grep "DIMM-R")" ]; then  
                echo $line
                tmp_read=`echo $line | awk '{print $4}'`
                tmp_write=`echo $line | awk '{print $11}'`
                update_bw=`echo $update_bw + $tmp_read | bc`
                write_bw=`echo $write_bw + $tmp_write | bc`
                total_bw=`echo $write_bw + $update_bw | bc`
            fi
        done < $datafile        
    done   
    echo $oneline_update >> $outfile_update_bw
    echo $oneline_update_r >> $outfile_update_bw_r
    echo $oneline_update_w >> $outfile_update_bw_w   
done

# bash analysis_cceh.sh

# python3 plot_update.py

# python3 plot_delete.py