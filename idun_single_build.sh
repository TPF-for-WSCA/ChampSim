#!/bin/bash

build_configs=("4k_max_region_configurations/btbx_tag_12_constant_size_pluss_2.json" "IDUN_CONFIGS/4k_max_region_configurations/btbx_tag_12_constant_size.json") #  "btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp" "btb_256_region_tag_exp")
# build_dir=("btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp")

old_dir=$(pwd)
cd ~/ChampSim/
for spec_dir in ${build_configs[@]}
do
    echo "Building experiment $spec_dir"
    echo -e "\tConfiguring ./IDUN_CONFIGS/${spec_dir}"
    ./config.sh ./IDUN_CONFIGS/$spec_dir
    echo -e "\tBuilding ./IDUN_CONFIGS/${spec_dir}"
    make -j &>> /cluster/work/romankb/build_$(basename ${spec_dir%.json}).log
done
cd $old_dir
