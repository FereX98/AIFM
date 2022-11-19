#!/bin/bash

source ../../../shared.sh

sudo cgcreate -t $USER -a $USER -g memory:/memctl
echo $$ > /sys/fs/cgroup/memory/memctl/tasks

cache_size=4096
log_folder=`pwd`

sudo pkill -9 main
make clean
make -j
#rerun_local_iokerneld_noht
mem_bytes_limit=`echo $(($(($cache_size * 1024))*1024))`
echo $mem_bytes_limit > /sys/fs/cgroup/memory/memctl/memory.limit_in_bytes
./main > $log_folder/log.$cache_size
#kill_local_iokerneld
