#!/bin/bash

build_configs=("idun_32k_8_way_data_config.json" "idun_vcl_buffer_64d_data_config.json")

old_dir=pwd
cd ~/ChampSim/
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    ~/ChampSim/config.sh ~/ChampSim/${build_script}
    make
done
cd $old_dir
