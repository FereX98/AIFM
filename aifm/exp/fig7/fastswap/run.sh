#!/bin/bash

#sudo sh -c "echo '+memory' > /sys/fs/cgroup/unified/cgroup.subtree_control"
#sudo sh -c "mkdir /sys/fs/cgroup/unified/bench"
#sudo sh -c "echo $$ > /sys/fs/cgroup/unified/bench/cgroup.procs"

sudo cgcreate -t $USER -a $USER -g memory:/memctl
echo $$ > /sys/fs/cgroup/memory/memctl/tasks

#cache_sizes=(31 29 27 25 23 21 19 17 15 13 11 9 7 5 3 1)
cache_sizes=(31)

mem_bytes_limit=`echo $(($((31000 * 1024))*1024))`
echo $mem_bytes_limit > /sys/fs/cgroup/memory/memctl/memory.limit_in_bytes

sudo pkill -9 main

log_folder=`pwd`
cd ../../../DataFrame/original/
#rm -rf build
#mkdir build
cd build
#cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-9 ..
#make -j

for cache_size in ${cache_sizes[@]}
do
    mem_bytes_limit=`echo $(($(($(($cache_size * 1024))*1024))*1024))`
    #sudo sh -c "echo $mem_bytes_limit > /sys/fs/cgroup/unified/bench/memory.high"
    echo $mem_bytes_limit > /sys/fs/cgroup/memory/memctl/memory.limit_in_bytes
    #sudo taskset -c 1 ./bin/main > $log_folder/log.$cache_size
    sudo ./bin/main > $log_folder/log.$cache_size
done
