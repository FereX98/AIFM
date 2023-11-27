#!/bin/bash

source ../../../shared.sh


num_threads_arr=(54)
target_mops_arr=(1)
local_mem_arr=(2560 5120 10240)
sudo pkill -9 main
for num_threads in ${num_threads_arr[@]}
do
    for target_mops in ${target_mops_arr[@]}
    do
        for local_mem in ${local_mem_arr[@]}
        do
            log_name=aifm-web-tempo-${local_mem}m-${num_threads}t-${target_mops}ol
            sed "s/constexpr static uint64_t kCacheSize = .*/constexpr static uint64_t kCacheSize = $local_mem * Region::kSize;/g" main.cpp -i
            sed "s/constexpr static double target_mops =.*/constexpr static double target_mops = $target_mops;/g" main.cpp -i
            sed "s/static uint32_t kNumMutatorThreads = .*/static uint32_t kNumMutatorThreads = $num_threads;/g" main.cpp -i
            sed "s|save_to_file(\".*\.raw\", all_lats);|save_to_file(\"logs/$log_name\.raw\", all_lats);|g" main.cpp -i
            make clean
            make -j
            rerun_local_iokerneld
            rerun_mem_server
            run_program ./main | tee -a logs/${log_name}
        done
    done
done
kill_local_iokerneld