#!/bin/bash

#build_configs=("saga_8k_config.json" "saga_64k_config.json" "saga_4k_config.json" "saga_32k_config.json" "saga_2k_config.json" "saga_1k_config.json" "saga_16k_config.json" "saga_128k_config.json" "saga_128m_config.json" "saga_vcl_8_config.json" "saga_vcl_12_config.json" "saga_vcl_16_config.json" "saga_vcl_8_aligned_config.json" "saga_vcl_12_aligned_config.json" "saga_vcl_16_aligned_config.json", "saga_vcl_buffer_config.json")

#build_configs=("saga_vcl_buffer_64d_config.json" "saga_64k_config.json" "saga_32k_config.json" "saga_128m_config.json" "saga_vcl_buffer_16d_config.json" "saga_vcl_buffer_16a_config.json" "saga_vcl_buffer_64d_arm_config.json" "saga_vcl_buffer_64d_config.json" "saga_vcl_buffer_16d_config.json" "saga_vcl_buffer_16a_config.json" ) 
#build_configs=("saga_vcl_buffer_64d_max_way_config.json" "saga_vcl_buffer_8f_config.json" "saga_vcl_buffer_16f_config.json" "saga_vcl_buffer_32f_config.json" "saga_vcl_buffer_64f_config.json" "saga_vcl_buffer_128f_config.json") 
#build_configs=("saga_32k_config.json" "saga_vcl_buffer_8f_config.json" "saga_vcl_buffer_4f_config.json" "saga_vcl_buffer_128f_config.json" "saga_vcl_buffer_64f_config.json" "saga_vcl_buffer_32f_config.json" "saga_vcl_buffer_16f_config.json" "saga_vcl_buffer_256f_config.json") 
build_configs=("saga_32k_fdip_config.json" ""saga_64k_fdip_config.json"" "saga_vcl_buffer_64d_fdip_config.json" "saga_vcl_buffer_64d_config.json") 
#build_configs=("saga_4k_data_config.json" "saga_8k_data_config.json" "saga_16k_data_config.json" "saga_32k_data_config.json" "saga_64k_data_config.json" "saga_128k_data_config.json" "saga_256k_data_config.json") 

old_dir=$(pwd)
cd /cluster/projects/nn4650k/workspace/ChampSim/
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    /cluster/projects/nn4650k/workspace/ChampSim/config.sh /cluster/projects/nn4650k/workspace/ChampSim/${build_script}
    make -j
done
cd $old_dir
