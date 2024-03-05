#!/bin/bash
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")

for b in ${benchmarks[@]}
do
    echo "Handling ${b}"
    touch ./${b}/complete_pc_offset_mapping.tsv
    old_dir=pwd
    cd ./${b}
    for result in ./**/*;
    do
        if [ $result = *cpu0_pc_offset_mapping.tsv ]
        then
            echo "\tAdding ${result}"
            cat $result >> ./${b}/complete_pc_offset_mapping.tsv
        fi
    done
    cd $old_dir
done