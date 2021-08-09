#!/usr/bin/env bash

# for i in cceh cceh30 clevel30 clht30 turbo turbo30 
for i in dash
do
    filename="all_loadfactor.$i"
    cmd="cat $filename | grep 'Load factor' | awk '{print \$3}' > $filename.parse"
    eval ${cmd}
done

python3 plot.py