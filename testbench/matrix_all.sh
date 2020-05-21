sudo ./mem_bench --type=fillseqNT --mode=matrix --thread=16  | tee fillseqNT_matrix.log
sudo ./mem_bench --type=fillseqWB --mode=matrix --thread=16  | tee fillseqWB_matrix.log
sudo ./mem_bench --type=fillrandomNT --mode=matrix --thread=16  | tee fillrandomNT_matrix.log
sudo ./mem_bench --type=fillrandomWB --mode=matrix --thread=16  | tee fillrandomWB_matrix.log
sudo ./mem_bench --type=readseqNT --mode=matrix --thread=16 --load=true --initfile=true | tee readseqNT_matrix.log
sudo ./mem_bench --type=readrandomNT --mode=matrix --thread=16 --load=true --initfile=true | tee readrandomNT_matrix.log
