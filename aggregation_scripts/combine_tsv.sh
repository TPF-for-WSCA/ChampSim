#!/bin/bash

output_file="raw_data/combined.tsv"

tsv_files=$(find . -type f -name "btb_dynamic_bit_information.tsv")

first_file=true

for file in $tsv_files; do
    if $first_file; then
        cat $file > $output_file
        first_file=false
    else
        tail -n +2 $file >> $output_file
    fi
done

