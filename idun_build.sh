#!/bin/bash

build_configs=("idun_32k_config.json" "idun_64k_config.json" "idun_128k_config.json" "idun_vcl_buffer_64d_fdip_config.json" "idun_vcl_buffer_64d_fdip_16way_config.json" "idun_vcl_buffer_64d_fdip_extendblcks_config.json" "idun_vcl_buffer_64d_fdip_extendblcks_bigway_config.json" "idun_vcl_buffer_64d_config.json")

#build_configs=("idun_vcl_buffer_64d_max_way_config.json")
#build_configs=("idun_vcl_buffer_64d_data_no_history_config.json" "idun_vcl_buffer_64d_data_partial_historyconfig.json" "idun_vcl_buffer_64d_no_history_config.json" "idun_vcl_buffer_64d_partial_history_config.json" "idun_vcl_buffer_64f_data_no_history_config.json" "idun_vcl_buffer_64f_data_partial_history_config.json" "idun_vcl_buffer_64f_no_history_config.json" "idun_vcl_buffer_64f_partial_history_config.json" "idun_vcl_buffer_64lru_8w_no_history_config.json" "idun_vcl_buffer_64lru_8w_partial_history_config.json" "idun_vcl_buffer_64lru_data_8w_no_history_config.json" "idun_vcl_buffer_64lru_data_8w_partial_history_config.json")
#build_configs=("idun_simple_vcl_buffer_64lru_8w_config.json" "idun_simple_vcl_buffer_64lru_8w_data_config.json" "idun_4k_config.json" "idun_8k_config.json" "idun_16k_config.json" "idun_32k_config.json" "idun_64k_config.json" "idun_128k_config.json" "idun_256k_config.json" "idun_4k_data_config.json" "idun_8k_data_config.json" "idun_16k_data_config.json" "idun_32k_data_config.json" "idun_64k_data_config.json" "idun_128k_data_config.json" "idun_256k_data_config.json" "idun_vcl_buffer_8f_config.json" "idun_vcl_buffer_4f_config.json" "idun_vcl_buffer_128f_config.json" "idun_vcl_buffer_64f_config.json" "idun_vcl_buffer_32f_config.json" "idun_vcl_buffer_16f_config.json" "idun_vcl_buffer_256f_config.json" "idun_vcl_buffer_64d_config.json" "idun_vcl_buffer_64d_data_config.json" "idun_vcl_buffer_64f_config.json" "idun_vcl_buffer_64f_data_config.json" "idun_vcl_buffer_64lru_2w_config.json" "idun_vcl_buffer_64lru_data_2w_config.json" "idun_vcl_buffer_64lru_4w_config.json" "idun_vcl_buffer_64lru_data_4w_config.json" "idun_vcl_buffer_64lru_8w_config.json" "idun_vcl_buffer_64lru_data_8w_config.json" "idun_vcl_buffer_128d_config.json" "idun_vcl_buffer_128d_data_config.json" "idun_vcl_buffer_128f_config.json" "idun_vcl_buffer_128f_data_config.json" "idun_vcl_buffer_128lru_2w_config.json" "idun_vcl_buffer_128lru_2w_data_config.json" "idun_vcl_buffer_128lru_4w_config.json" "idun_vcl_buffer_128lru_4w_data_config.json" "idun_vcl_buffer_128lru_8w_config.json" "idun_vcl_buffer_128lru_8w_data_config.json") 
#build_configs+=("idun_32k_3lip_config.json" "idun_32k_2lip_config.json" "idun_32k_4lip_config.json" "idun_32k_data_2lip_config.json" "idun_32k_data_3lip_config.json" "idun_32k_data_4lip_config.json" "idun_32k_data_lip_config.json" "idun_32k_lip_config.json" "idun_64k_2lip_config.json" "idun_64k_3lip_config.json" "idun_64k_4lip_config.json" "idun_64k_data_2lip_config.json" "idun_64k_data_3lip_config.json" "idun_64k_data_4lip_config.json" "idun_64k_data_lip_config.json" "idun_64k_lip_config.json" "idun_vcl_buffer_64d_3lip_config.json" "idun_vcl_buffer_64d_4lip_config.json" "idun_vcl_buffer_64d_data_3lip_config.json" "idun_vcl_buffer_64d_data_4lip_config.json" "idun_vcl_buffer_64d_data_lip_config.json" "idun_vcl_buffer_64d_lip_config.json" "idun_vcl_buffer_64f_3lip_config.json" "idun_vcl_buffer_64f_4lip_config.json" "idun_vcl_buffer_64f_data_3lip_config.json" "idun_vcl_buffer_64f_data_4lip_config.json" "idun_vcl_buffer_64f_data_lip_config.json" "idun_vcl_buffer_64f_lip_config.json" "idun_vcl_buffer_64lru_8w_3lip_config.json" "idun_vcl_buffer_64lru_8w_4lip_config.json" "idun_vcl_buffer_64lru_8w_lip_config.json" "idun_vcl_buffer_64lru_data_8w_3lip_config.json" "idun_vcl_buffer_64lru_data_8w_4lip_config.json" "idun_vcl_buffer_64lru_data_8w_lip_config.json" "idun_vcl_buffer_64d_lru_data_config.json")
#build_configs=("idun_vcl_buffer_128lru_8w_config.json" "idun_vcl_buffer_128lru_8w_data_config.json") 
#build_configs=("idun_32k_data_config.json" "idun_64k_data_config.json" "idun_128k_data_config.json" "idun_vcl_buffer_64d_data_config.json")
old_dir=$(pwd)
cd ~/ChampSim/
for build_script in ${build_configs[@]}
do
    echo "Building ${build_script}"
    ~/ChampSim/config.sh ~/ChampSim/${build_script}
    make -j
done
cd $old_dir


for job in `jobs -p`
do
    echo $job
    wait $job
done

