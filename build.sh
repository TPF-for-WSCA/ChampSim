#!/bin/bash

build_configs=("saga_8k_config.json" "saga_64k_config.json" "saga_4k_config.json" "saga_32k_config.json" "saga_2k_config.json" "saga_1k_config.json" "saga_16k_config.json" "saga_128k_config.json" "saga_bigcache_config.json")

old_dir=pwd
cd ~/workspace/ChampSim/
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    ~/workspace/ChampSim/config.sh ~/workspace/ChampSim/${build_script}
    make
done
cd $old_dir
