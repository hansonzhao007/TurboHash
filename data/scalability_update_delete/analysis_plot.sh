#!/bin/bash


rm *.parse

outfile_load="scalability_load.parse"
outfile_update="scalability_update.parse"
outfile_delete="scalability_delete.parse"
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_update
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_delete

outfile_load_io="scalability_load_io.parse"
outfile_load_io_r="scalability_load_io_r.parse"
outfile_load_io_w="scalability_load_io_w.parse"
outfile_update_io="scalability_update_io.parse"
outfile_update_io_r="scalability_update_io_r.parse"
outfile_update_io_w="scalability_update_io_w.parse"
outfile_delete_io="scalability_delete_io.parse"
outfile_delete_io_r="scalability_delete_io_r.parse"
outfile_delete_io_w="scalability_delete_io_w.parse"
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load_io
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load_io_r
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load_io_w
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_update_io
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_update_io_r
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_update_io_w
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_delete_io
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_delete_io_r
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_delete_io_w

outfile_load_bw="scalability_load_bw.parse"
outfile_load_bw_r="scalability_load_bw_r.parse"
outfile_load_bw_w="scalability_load_bw_w.parse"
outfile_update_bw="scalability_update_bw.parse"
outfile_update_bw_r="scalability_update_bw_r.parse"
outfile_update_bw_w="scalability_update_bw_w.parse"
outfile_delete_bw="scalability_delete_bw.parse"
outfile_delete_bw_r="scalability_delete_bw_r.parse"
outfile_delete_bw_w="scalability_delete_bw_w.parse"
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load_bw
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load_bw_r
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_load_bw_w
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_update_bw
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_update_bw_r
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_update_bw_w
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_delete_bw
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_delete_bw_r
echo "thread,clevel30,clht30,turbo,turbo30" >> $outfile_delete_bw_w

for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # parse the throughput
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

    # parse the io
    oneline_load="$t"
    oneline_update="$t"
    oneline_delete="$t"
    oneline_load_r="$t"
    oneline_load_w="$t"
    oneline_update_r="$t"
    oneline_update_w="$t"
    oneline_delete_r="$t"
    oneline_delete_w="$t"
    for i in clevel30 clht30 turbo turbo30
    do
        datafile="thread_update_delete.${i}_${t}"
        update_io=0.0
        write_io=0.0
        total_io=0.0
        index=0
        while read line; do
            if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
                if [[ $index -eq 1 ]]; then                   
                    oneline_load_r="$oneline_load_r,$update_io"
                    oneline_load_w="$oneline_load_w,$write_io"
                    oneline_load="$oneline_load,$total_io"
                elif [[ $index -eq 2 ]]; then
                    oneline_update_r="$oneline_update_r,$update_io"
                    oneline_update_w="$oneline_update_w,$write_io"
                    oneline_update="$oneline_update,$total_io"
                elif [[ $index -eq 4 ]]; then
                    oneline_delete_r="$oneline_delete_r,$update_io"
                    oneline_delete_w="$oneline_delete_w,$write_io"
                    oneline_delete="$oneline_delete,$total_io"
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
    echo $oneline_load >> $outfile_load_io
    echo $oneline_load_r >> $outfile_load_io_r
    echo $oneline_load_w >> $outfile_load_io_w
    echo $oneline_update >> $outfile_update_io
    echo $oneline_update_r >> $outfile_update_io_r
    echo $oneline_update_w >> $outfile_update_io_w
    echo $oneline_delete >> $outfile_delete_io
    echo $oneline_delete_r >> $outfile_delete_io_r
    echo $oneline_delete_w >> $outfile_delete_io_w

    # Parse the bw
    oneline_load="$t"
    oneline_update="$t"
    oneline_delete="$t"
    oneline_load_r="$t"
    oneline_load_w="$t"
    oneline_update_r="$t"
    oneline_update_w="$t"
    oneline_delete_r="$t"
    oneline_delete_w="$t"
    for i in clevel30 clht30 turbo turbo30
    do
        datafile="thread_update_delete.${i}_${t}"
        update_bw=0.0
        write_bw=0.0
        total_bw=0.0
        index=0
        while read line; do
            if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then   
                if [[ $index -eq 1 ]]; then                   
                    oneline_load_r="$oneline_load_r,$update_bw"
                    oneline_load_w="$oneline_load_w,$write_bw"
                    oneline_load="$oneline_load,$total_bw"
                elif [[ $index -eq 2 ]]; then
                    oneline_update_r="$oneline_update_r,$update_bw"
                    oneline_update_w="$oneline_update_w,$write_bw"
                    oneline_update="$oneline_update,$total_bw"
                elif [[ $index -eq 4 ]]; then
                    oneline_delete_r="$oneline_delete_r,$update_bw"
                    oneline_delete_w="$oneline_delete_w,$write_bw"
                    oneline_delete="$oneline_delete,$total_bw"
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
    echo $oneline_load >> $outfile_load_bw
    echo $oneline_load_r >> $outfile_load_bw_r
    echo $oneline_load_w >> $outfile_load_bw_w
    echo $oneline_update >> $outfile_update_bw
    echo $oneline_update_r >> $outfile_update_bw_r
    echo $oneline_update_w >> $outfile_update_bw_w
    echo $oneline_delete >> $outfile_delete_bw
    echo $oneline_delete_r >> $outfile_delete_bw_r
    echo $oneline_delete_w >> $outfile_delete_bw_w
done

bash _analysis.sh

python3 plot.py

python3 _plot.py