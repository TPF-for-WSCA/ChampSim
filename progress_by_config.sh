#!/bin/bash

total=$(($(ls -l $2 | wc -l) - 1))
for dir in $1/*; do
	if [[ -f $dir ]]; then
		continue
	fi
	echo "Progress for $(basename $dir)"
	echo -e "\t$(find $dir -name '*.txt' | wc -l) / $total"
done
