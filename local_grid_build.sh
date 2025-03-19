#!/bin/bash

build_dir=("small_scale")

old_dir=$(pwd)
for spec_dir in ${build_dir[@]}
do
    for build_script in ./LOCAL_CONFIGS/$spec_dir/*;
    do
        echo "Building ${build_script}"
        ./config.sh $build_script
        make -j
    done
done