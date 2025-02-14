#!/bin/bash

build_dir=("btb_region_tag_exp" "btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp" "btb_256_region_tag_exp")
# build_dir=("btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp")

old_dir=$(pwd)
cd ~/ChampSim/
for spec_dir in ${build_dir[@]}
do
    echo "Building experiment $spec_dir"
    for build_script in ./IDUN_CONFIGS/$spec_dir/*;
    do
        echo -e "\tConfiguring ${build_script}"
        ./config.sh $build_script
        echo -e "\tBuilding ${build_script}"
        make -j &> /cluster/work/romankb/build_${build_script%.json}.log
    done
done
cd $old_dir
