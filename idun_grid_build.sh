#!/bin/bash


build_dir=("full_btb_grid_search" "region_sensitivity" "region_design_space_exploration") # ("region_sampling" "full_btb_grid_search") # "4k_12b_region_tag_sensitivity" "4k_10b_region_tag_sensitivity") #  "btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp" "btb_256_region_tag_exp")
# build_dir=("btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp")

old_dir=$(pwd)
cd ~/ChampSim/
for spec_dir in ${build_dir[@]}
do
    echo "Building experiment $spec_dir"
    for build_script in ./IDUN_CONFIGS/$spec_dir/*;
    do
        if ! [[ -f "$build_script" ]]; then
            continue
        fi
        echo -e "\tConfiguring ${build_script}"
        ./config.sh $build_script
        echo -e "\tBuilding ${build_script}"
        make -j 16 &>> /cluster/work/romankb/build_$(basename ${build_script%.json}).log
    done
done
cd $old_dir
