#!/bin/bash

build_configs=("idun_8k_config.json" "idun_64k_config.json" "idun_4k_config.json" "idun_32k_config.json" "idun_2k_config.json" "idun_1k_config.json" "idun_16k_config.json" "idun_128k_config.json" "idun_bigcache_config.json" "idun_vcl_buffer_config.json")

old_dir=pwd
cd ~/ChampSim/
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    ~/ChampSim/config.sh ~/ChampSim/${build_script}
    make
done
cd $old_dir
