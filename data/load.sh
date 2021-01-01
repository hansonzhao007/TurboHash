#!/usr/bin/env bash

touch load.csv
for i in {0..7}
do
    keyword="thread $i"
    echo "------ $keyword -------"
    cmd="cat load.data | grep '${keyword}' | awk '{print \$9}' | awk -F',' '{print \$1}' > tmp.txt"
    eval ${cmd}
    paste -d" " tmp.txt load.csv | tee load.csv
done

python3 load.py