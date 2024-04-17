#!/bin/bash

#build_configs=("saga_8k_config.json" "saga_64k_config.json" "saga_4k_config.json" "saga_32k_config.json" "saga_2k_config.json" "saga_1k_config.json" "saga_16k_config.json" "saga_128k_config.json" "saga_128m_config.json" "saga_vcl_8_config.json" "saga_vcl_12_config.json" "saga_vcl_16_config.json" "saga_vcl_8_aligned_config.json" "saga_vcl_12_aligned_config.json" "saga_vcl_16_aligned_config.json", "saga_vcl_buffer_config.json")

#build_configs=("saga_vcl_buffer_64d_config.json" "saga_64k_config.json" "saga_32k_config.json" "saga_128m_config.json" "saga_vcl_buffer_16d_config.json" "saga_vcl_buffer_16a_config.json" "saga_vcl_buffer_64d_arm_config.json" "saga_vcl_buffer_64d_config.json" "saga_vcl_buffer_16d_config.json" "saga_vcl_buffer_16a_config.json" ) 
#build_configs=("saga_vcl_buffer_64d_max_way_config.json" "saga_vcl_buffer_8f_config.json" "saga_vcl_buffer_16f_config.json" "saga_vcl_buffer_32f_config.json" "saga_vcl_buffer_64f_config.json" "saga_vcl_buffer_128f_config.json") 
#build_configs=("saga_32k_config.json" "saga_vcl_buffer_8f_config.json" "saga_vcl_buffer_4f_config.json" "saga_vcl_buffer_128f_config.json" "saga_vcl_buffer_64f_config.json" "saga_vcl_buffer_32f_config.json" "saga_vcl_buffer_16f_config.json" "saga_vcl_buffer_256f_config.json") 
#build_configs=("saga_32k_config.json" "saga_32k_fdip_config.json" "saga_32k_fdip_btb2048_config.json" "saga_32k_fdip_btb8192_config.json" "saga_vcl_buffer_64d_fdip_config.json" "saga_vcl_buffer_64d_fdip_btb2048_config.json" "saga_vcl_buffer_64d_fdip_btb8192_config.json" "saga_vcl_buffer_64d_config.json") 
#build_configs=("saga_ubs_10_small_ways.json" "saga_ubs_10_small_ways_not_extending.json" "saga_ubs_10_ways.json" "saga_ubs_10_ways_not_extending.json" "saga_ubs_12_small_ways.json" "saga_ubs_12_small_ways_not_extending.json" "saga_ubs_14_small_ways.json" "saga_ubs_14_small_ways_not_extending.json")
#build_configs=("saga_4k_data_config.json" "saga_8k_data_config.json" "saga_16k_data_config.json" "saga_32k_data_config.json" "saga_64k_data_config.json" "saga_128k_data_config.json" "saga_256k_data_config.json") 
#build_configs=("saga_ubs_17_ways.json" "saga_ubs_17_ways_not_extending.json" "saga_ubs_8_ways.json" "saga_ubs_8_ways_not_extending.json")
#build_configs=("saga_vcl_64k_64s_extending.json" "saga_vcl_64k_64s_not_extending.json" "saga_128k_config.json")
#build_configs=("saga_ubsBOUND1.json" "saga_ubsBOUND2.json" "saga_ubsBOUND6.json" "saga_ubsBOUND8.json" "saga_ubsDEFAULT.json" "saga_ubsINSERT1.json" "saga_ubsINSERT3.json" "saga_ubsINSERT4.json")
#build_configs=("saga_ubs_10_ways.json" "saga_ubs_10_ways_not_extending.json" "saga_ubs_10_small_ways.json" "saga_ubs_10_small_ways_not_extending.json")
build_configs=("saga_ubs_overhead_isca_lru.json" "saga_ubs_overhead_isca_extended_lru.json")
cd /cluster/projects/nn4650k/workspace/ChampSim/
old_dir=$(pwd)
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    ./config.sh ./SAGA_CONFIGS/latency_sensitivity/${build_script}
    make -j
done
cd $old_dir
