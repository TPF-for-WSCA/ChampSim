#!/bin/bash

build_dir=("google")

old_dir=$(pwd)
cd ~/ChampSim/
for spec_dir in ${build_dir[@]}
do
    for build_script in ./IDUN_CONFIGS/$spec_dir/*;
    do
        echo "Building ${build_script}"
        ./config.sh $build_script
        make -j
    done
done
cd $old_dir
