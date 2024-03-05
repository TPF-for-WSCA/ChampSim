#!/bin/bash
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")

echo $(pwd)
for b in ${benchmarks[@]}
do
    echo "Handling ${b}"
    touch ./${b}/complete_pc_offset_mapping.tsv
    old_dir=$(pwd)
    cd ./${b}
    for result in ./**/**/*;
    do
        if [ -z ${result##*pc_offset_mapping.tsv} ];
        then
            echo -e "\tAdding ${result}"
            cat $result >> ./complete_pc_offset_mapping.tsv
        fi
    done
    cd $old_dir
done
