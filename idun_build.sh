#!/bin/bash

#build_configs=("idun_vcl_buffer_64d_max_way_config.json")
build_configs=("idun_4k_config.json" "idun_8k_config.json" "idun_16k_config.json" "idun_32k_config.json" "idun_64k_config.json" "idun_128k_config.json" "idun_256k_config.json" "idun_4k_data_config.json" "idun_8k_data_config.json" "idun_16k_data_config.json" "idun_32k_data_config.json" "idun_64k_data_config.json" "idun_128k_data_config.json" "idun_256k_data_config.json" "idun_vcl_buffer_8f_config.json" "idun_vcl_buffer_4f_config.json" "idun_vcl_buffer_128f_config.json" "idun_vcl_buffer_64f_config.json" "idun_vcl_buffer_32f_config.json" "idun_vcl_buffer_16f_config.json" "idun_vcl_buffer_256f_config.json" "idun_vcl_buffer_64d_max_way_config.json") 

old_dir=pwd
cd ~/ChampSim/
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    ~/ChampSim/config.sh ~/ChampSim/${build_script}
    make
done
cd $old_dir
