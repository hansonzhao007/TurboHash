#!/bin/bash

rm *.parse

outfile="rehashlat.parse"
echo "bucket_count,lat16,lat30" >> $outfile
# Parse the lat
for i in 16 32 64 128 256 512 1024 2048 4096 8192
do
    datafile1="rehashlat${i}"
    datafile2="rehashlat2_${i}"
    
    lat=""
    while read line; do
        if [ -n "$(echo $line | grep "MinorRehash avglat:")" ]; then  
            lat=`echo $line |  awk '{print $3}'`
        fi
    done < $datafile1

    while read line; do
        if [ -n "$(echo $line | grep "MinorRehash avglat:")" ]; then  
            tmp=`echo $line |  awk '{print $3}'`
            lat="$lat,$tmp"
        fi
    done < $datafile2

    echo "$i,$lat" >> $outfile
done

python3 plot.py