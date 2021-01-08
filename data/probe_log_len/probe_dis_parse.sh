#!/usr/bin/env bash

for i in {1..9}
do
    filename="probe_dis_${i}0per.data"
    echo "------ $filename -------"
    for j in {1..16}
    do
        cmd="tail -n +29 $filename | grep -v 'stats' | grep -v 'Bucket' | grep -w -c $j >> ${filename}.parse"
        eval ${cmd}
    done
done
