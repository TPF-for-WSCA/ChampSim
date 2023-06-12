#!/bin/bash

#build_configs=("idun_64k_config.json" "idun_32k_config.json" "idun_128m_config.json" "idun_vcl_buffer_64d_max_way_config.json")
build_configs=("idun_vcl_buffer_64d_max_way_config.json")

old_dir=pwd
cd ~/ChampSim/
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    ~/ChampSim/config.sh ~/ChampSim/${build_script}
    make
done
cd $old_dir
