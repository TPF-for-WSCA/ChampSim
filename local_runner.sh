#!/bin/bash

binary_dir=("small_scale")
count=0


for dir in ${binary_dir[@]}
do
    echo "Running ${dir}"
    for bin in ./bin/$dir/*;
    do
        echo "\tExp bin: ${bin}"
        # TODO: which traces to execute with?
    done
done