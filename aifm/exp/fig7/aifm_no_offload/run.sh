#!/bin/bash

log_folder=`pwd`

cd ../../../
source shared.sh

#cache_sizes=(1 3 5 7 9 11 13 15 17 19 21 23 25 27 29 31)
cache_sizes=(31)

#75%
#cache_sizes=(23)
#50%
#cache_sizes=(15)
#25%
#cache_sizes=(8)


recompile_aifm=$1
recompile_dataframe=$1

sudo pkill -9 main

if [ "$recompile_aifm" = t ]; then
#CXXFLAGS="-DDISABLE_OFFLOAD_UNIQUE -DDISABLE_OFFLOAD_COPY_DATA_BY_IDX -DDISABLE_OFFLOAD_SHUFFLE_DATA_BY_IDX -DDISABLE_OFFLOAD_ASSIGN -DDISABLE_OFFLOAD_AGGREGATE -DSTW_GC"
CXXFLAGS="-DDISABLE_OFFLOAD_UNIQUE -DDISABLE_OFFLOAD_COPY_DATA_BY_IDX -DDISABLE_OFFLOAD_SHUFFLE_DATA_BY_IDX -DDISABLE_OFFLOAD_ASSIGN -DDISABLE_OFFLOAD_AGGREGATE"
make clean
make -j CXXFLAGS="$CXXFLAGS"
fi

cd DataFrame/AIFM/
if [ "$recompile_dataframe" = t ]; then
rm -rf build
mkdir build
cd build
#config debug
cmake -E env CXXFLAGS="$CXXFLAGS" cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-9 ..
cd ..
fi

for cache_size in ${cache_sizes[@]}
do
    #sed "s/constexpr uint64_t kCacheMBs.*/constexpr uint64_t kCacheMBs = $cache_size;/g" app/main_tcp.cc -i
    sed "s/constexpr uint64_t kCacheGBs.*/constexpr uint64_t kCacheGBs = $cache_size;/g" app/main_tcp.cc -i
    cd build
    if [ "$recompile_dataframe" = t ]; then
    make clean
    make -j
    fi
    rerun_local_iokerneld_noht
    rerun_mem_server
    run_program_noht ./bin/main_tcp 1>$log_folder/log.$cache_size 2>&1
    cd ..
done
kill_local_iokerneld
