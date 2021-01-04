#!/usr/bin/env bash
# use 'numactl' to run this script on a single socket

THREAD_NUM=8
SOCKET_NO=0
numactl -N $SOCKET_NO sudo ./mem_bench --type=fillseqNT --mode=matrix --thread=$THREAD_NUM  | tee fillseqNT_matrix.log
numactl -N $SOCKET_NO sudo ./mem_bench --type=fillseqWB --mode=matrix --thread=$THREAD_NUM  | tee fillseqWB_matrix.log
numactl -N $SOCKET_NO sudo ./mem_bench --type=fillrandomNT --mode=matrix --thread=$THREAD_NUM  | tee fillrandomNT_matrix.log
numactl -N $SOCKET_NO sudo ./mem_bench --type=fillrandomWB --mode=matrix --thread=$THREAD_NUM  | tee fillrandomWB_matrix.log
numactl -N $SOCKET_NO sudo ./mem_bench --type=readseqNT --mode=matrix --thread=$THREAD_NUM --load=true --initfile=true | tee readseqNT_matrix.log
numactl -N $SOCKET_NO sudo ./mem_bench --type=readrandomNT --mode=matrix --thread=$THREAD_NUM --load=true --initfile=true | tee readrandomNT_matrix.log
