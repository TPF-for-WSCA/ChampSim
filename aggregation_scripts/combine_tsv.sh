#!/bin/bash

output_file="raw_data/combined_region_counts.tsv"

tsv_files=$(find ./ipc1_server -type f -name "overall_max_region.tsv")

first_file=true

for file in $tsv_files; do
    if $first_file; then
        # cat $file > $output_file
                head -n 1 "$file" | awk 'BEGIN {OFS="\t"} {print "source_file", $0}' >> "$output_file"
        tail -n +2 "$file" | awk -v f="$file" 'BEGIN {OFS="\t"} {print f, $0}' >> "$output_file"
        first_file=false
    else
        # tail -n +2 $file >> $output_file
                tail -n +2 "$file" | awk -v f="$file" 'BEGIN {OFS="\t"} {print f, $0}' >> "$output_file"
    fi
done
