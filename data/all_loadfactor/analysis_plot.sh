#!/usr/bin/env bash

for i in cceh dash turbo cceh30 clevel30 clht30 turbo30 
do
    filename="all_loadfactor.$i"
    cmd="cat $filename | grep 'Load factor' | awk '{print \$3}' > $filename.parse"
    eval ${cmd}
done

python3 plot.py

rm *.parse