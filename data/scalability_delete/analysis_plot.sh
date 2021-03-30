#!/bin/bash


rm *.parse

outfile_delete="scalability_delete.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete

outfile_delete_io="scalability_delete_io.parse"
outfile_delete_io_r="scalability_delete_io_r.parse"
outfile_delete_io_w="scalability_delete_io_w.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_io
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_io_r
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_io_w

outfile_delete_bw="scalability_delete_bw.parse"
outfile_delete_bw_r="scalability_delete_bw_r.parse"
outfile_delete_bw_w="scalability_delete_bw_w.parse"
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_bw
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_bw_r
echo "thread,clevel30,clht30,turbo,turbo30,dash" >> $outfile_delete_bw_w

for t in 1 2 4 8 16 20 24 28 32 36 40
do
    # parse the throughput
    oneline_delete_iops="$t"
    # parse the io
    oneline_delete_io="$t"
    oneline_delete_r_io="$t"
    oneline_delete_w_io="$t"
    # Parse the bw    
    oneline_delete_bw="$t"    
    oneline_delete_r_bw="$t"
    oneline_delete_w_bw="$t"

    for i in clevel30 clht30 turbo turbo30 dash
    do        
        datafile="thread_delete.${i}_${t}"
        echo "parse ${datafile}"
        update_io=0.0
        write_io=0.0
        total_io=0.0
        update_bw=0.0
        write_bw=0.0
        total_bw=0.0
        index=0

        while read line; do   
            if [ -n "$(echo $line | grep "Start IPMWatcher for")" ]; then                   
                update_io=0.0
                write_io=0.0
                total_io=0.0
                update_bw=0.0
                write_bw=0.0
                total_bw=0.0
                index=$((index + 1))
            fi

            if [ -n "$(echo $line | grep "MB |")" ]; then  
                tmp_read=`echo $line | awk '{print $7}'`
                tmp_write=`echo $line | awk '{print $9}'`
                update_io=`echo $update_io + $tmp_read | bc`
                write_io=`echo $write_io + $tmp_write | bc`
                total_io=`echo $write_io + $update_io | bc`
            fi

            if [ -n "$(echo $line | grep "DIMM-R")" ]; then                
                tmp_read=`echo $line | awk '{print $4}'`
                tmp_write=`echo $line | awk '{print $11}'`
                update_bw=`echo $update_bw + $tmp_read | bc`
                write_bw=`echo $write_bw + $tmp_write | bc`
                total_bw=`echo $write_bw + $update_bw | bc`
            fi

            if [ -n "$(echo $line | grep "delete ")" ]; then          
                tmp=`echo $line | awk '{print$5}'`
                oneline_delete_iops="$oneline_delete_iops,$tmp"
            fi

            if [ -n "$(echo $line | grep "Destroy IPMWatcher")" ]; then
                if [[ $index -eq 1 ]]; then
                    oneline_delete_r_io="$oneline_delete_r_io,$update_io"
                    oneline_delete_w_io="$oneline_delete_w_io,$write_io"
                    oneline_delete_io="$oneline_delete_io,$total_io"
                    oneline_delete_r_bw="$oneline_delete_r_bw,$update_bw"
                    oneline_delete_w_bw="$oneline_delete_w_bw,$write_bw"
                    oneline_delete_bw="$oneline_delete_bw,$total_bw"
                fi
            fi
        done < $datafile
        echo $oneline_delete_iops
        echo $oneline_delete_io
        echo $oneline_delete_bw
    done
    echo $oneline_delete_iops >> $outfile_delete
    echo $oneline_delete_io >> $outfile_delete_io
    echo $oneline_delete_r_io >> $outfile_delete_io_r
    echo $oneline_delete_w_io >> $outfile_delete_io_w
    echo $oneline_delete_bw >> $outfile_delete_bw
    echo $oneline_delete_r_bw >> $outfile_delete_bw_r
    echo $oneline_delete_w_bw >> $outfile_delete_bw_w

done

python3 plot.py