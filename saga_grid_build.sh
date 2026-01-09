#!/bin/bash

#build_configs=("saga_8k_config.json" "saga_64k_config.json" "saga_4k_config.json" "saga_32k_config.json" "saga_2k_config.json" "saga_1k_config.json" "saga_16k_config.json" "saga_128k_config.json" "saga_128m_config.json" "saga_vcl_8_config.json" "saga_vcl_12_config.json" "saga_vcl_16_config.json" "saga_vcl_8_aligned_config.json" "saga_vcl_12_aligned_config.json" "saga_vcl_16_aligned_config.json", "saga_vcl_buffer_config.json")


# build_dir=("btb_1k_region_tag_exp" "btb_4k_region_tag_exp")
# build_dir=("btb_4k_region_tag_set_associative_set_idx")
# build_dir=("small_set_configurations")
build_dir=("mpki_study")

old_dir=$(pwd)
cd /cluster/projects/nn4650k/workspace/ChampSim/
for spec_dir in ${build_dir[@]}
do
    for build_script in ./SAGA_CONFIGS/$spec_dir/*;
    do
        echo "Building ${build_script}"
        ./config.sh $build_script
        make -j 16 &>> /cluster/work/users/romankb/build_$(basename ${build_script%.json}).log
    done
done
cd $old_dir
