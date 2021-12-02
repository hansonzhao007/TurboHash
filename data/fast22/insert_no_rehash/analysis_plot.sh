#!/bin/bash


rm *.parse

outfile_load="scalability_load.parse"
outfile_read="scalability_read.parse"
outfile_readnon="scalability_readnon.parse"
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_load
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_read
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_readnon

outfile_load_io="scalability_load_io.parse"
outfile_load_io_r="scalability_load_io_r.parse"
outfile_load_io_w="scalability_load_io_w.parse"
outfile_read_io="scalability_read_io.parse"
outfile_read_io_r="scalability_read_io_r.parse"
outfile_read_io_w="scalability_read_io_w.parse"
outfile_readnon_io="scalability_readnon_io.parse"
outfile_readnon_io_r="scalability_readnon_io_r.parse"
outfile_readnon_io_w="scalability_readnon_io_w.parse"
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_load_io
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_load_io_r
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_load_io_w
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_read_io
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_read_io_r
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_read_io_w
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_readnon_io
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_readnon_io_r
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_readnon_io_w

outfile_load_bw="scalability_load_bw.parse"
outfile_load_bw_r="scalability_load_bw_r.parse"
outfile_load_bw_w="scalability_load_bw_w.parse"
outfile_read_bw="scalability_read_bw.parse"
outfile_read_bw_r="scalability_read_bw_r.parse"
outfile_read_bw_w="scalability_read_bw_w.parse"
outfile_readnon_bw="scalability_readnon_bw.parse"
outfile_readnon_bw_r="scalability_readnon_bw_r.parse"
outfile_readnon_bw_w="scalability_readnon_bw_w.parse"
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_load_bw
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_load_bw_r
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_load_bw_w
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_read_bw
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_read_bw_r
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_read_bw_w
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_readnon_bw
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_readnon_bw_r
echo "thread,cceh,cceh30,clevel30,clht30,turbo,turbo30,dash" >> $outfile_readnon_bw_w

for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # parse the throughput
    oneline_load="$t"
    oneline_read="$t"
    oneline_readnon="$t"
    for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread.${i}_${t}"
        echo $datafile
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
        echo $oneline_load                
    done
    echo $oneline_load >> $outfile_load
    echo $oneline_read >> $outfile_read
    echo $oneline_readnon >> $outfile_readnon

    # parse the io
    oneline_load="$t"
    oneline_read="$t"
    oneline_readnon="$t"
    oneline_load_r="$t"
    oneline_load_w="$t"
    oneline_read_r="$t"
    oneline_read_w="$t"
    oneline_readnon_r="$t"
    oneline_readnon_w="$t"
    for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread.${i}_${t}"
        read_io=0.0
        write_io=0.0
        total_io=0.0
        index=0
        while read line; do
            if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
                if [[ $index -eq 1 ]]; then                   
                    oneline_load_r="$oneline_load_r,$read_io"
                    oneline_load_w="$oneline_load_w,$write_io"
                    oneline_load="$oneline_load,$total_io"
                elif [[ $index -eq 2 ]]; then
                    oneline_read_r="$oneline_read_r,$read_io"
                    oneline_read_w="$oneline_read_w,$write_io"
                    oneline_read="$oneline_read,$total_io"
                fi
                read_io=0.0
                write_io=0.0
                total_io=0.0
                index=$((index + 1))
            fi

            if [ -n "$(echo $line | grep "MB |")" ]; then  
                tmp_read=`echo $line | awk '{print $7}'`
                tmp_write=`echo $line | awk '{print $9}'`
                read_io=`echo $read_io + $tmp_read | bc`
                write_io=`echo $write_io + $tmp_write | bc`
                total_io=`echo $write_io + $read_io | bc`
            fi
        done < $datafile
        oneline_readnon_r="$oneline_readnon_r,$read_io"
        oneline_readnon_w="$oneline_readnon_w,$write_io"
        oneline_readnon="$oneline_readnon,$total_io"
    done
    echo $oneline_load >> $outfile_load_io
    echo $oneline_load_r >> $outfile_load_io_r
    echo $oneline_load_w >> $outfile_load_io_w
    echo $oneline_read >> $outfile_read_io
    echo $oneline_read_r >> $outfile_read_io_r
    echo $oneline_read_w >> $outfile_read_io_w
    echo $oneline_readnon >> $outfile_readnon_io
    echo $oneline_readnon_r >> $outfile_readnon_io_r
    echo $oneline_readnon_w >> $outfile_readnon_io_w

    # Parse the bw
    oneline_load="$t"
    oneline_read="$t"
    oneline_readnon="$t"
    oneline_load_r="$t"
    oneline_load_w="$t"
    oneline_read_r="$t"
    oneline_read_w="$t"
    oneline_readnon_r="$t"
    oneline_readnon_w="$t"
    for i in cceh cceh30 clevel30 clht30 turbo turbo30 dash
    do
        datafile="thread.${i}_${t}"
        read_bw=0.0
        write_bw=0.0
        total_bw=0.0
        index=0
        while read line; do
            if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
                if [[ $index -eq 1 ]]; then                   
                    oneline_load_r="$oneline_load_r,$read_bw"
                    oneline_load_w="$oneline_load_w,$write_bw"
                    oneline_load="$oneline_load,$total_bw"
                elif [[ $index -eq 2 ]]; then
                    oneline_read_r="$oneline_read_r,$read_bw"
                    oneline_read_w="$oneline_read_w,$write_bw"
                    oneline_read="$oneline_read,$total_bw"
                fi
                read_bw=0.0
                write_bw=0.0
                total_bw=0.0
                index=$((index + 1))
            fi

            if [ -n "$(echo $line | grep "DIMM-R")" ]; then  
                echo $line
                tmp_read=`echo $line | awk '{print $4}'`
                tmp_write=`echo $line | awk '{print $11}'`
                read_bw=`echo $read_bw + $tmp_read | bc`
                write_bw=`echo $write_bw + $tmp_write | bc`
                total_bw=`echo $write_bw + $read_bw | bc`
            fi
        done < $datafile
        oneline_readnon_r="$oneline_readnon_r,$read_bw"
        oneline_readnon_w="$oneline_readnon_w,$write_bw"
        oneline_readnon="$oneline_readnon,$total_bw"
    done
    echo $oneline_load >> $outfile_load_bw
    echo $oneline_load_r >> $outfile_load_bw_r
    echo $oneline_load_w >> $outfile_load_bw_w
    echo $oneline_read >> $outfile_read_bw
    echo $oneline_read_r >> $outfile_read_bw_r
    echo $oneline_read_w >> $outfile_read_bw_w
    echo $oneline_readnon >> $outfile_readnon_bw
    echo $oneline_readnon_r >> $outfile_readnon_bw_r
    echo $oneline_readnon_w >> $outfile_readnon_bw_w
done

python3 plot.py