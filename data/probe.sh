#!/usr/bin/env bash

for i in 2 4 8 16 32 64
do
    filename="probe${i}.data"
    cmd="cat $filename | awk '{print \$12, \$19}' > $filename.parse"
    eval ${cmd}
done

python3 probe.py