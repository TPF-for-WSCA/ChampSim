#!/bin/bash

print_progress() {
	total=$(($(find $2 -name '*.xz' -o -name '*.gz' | wc -l)))
	for dir in $1/*; do
		if [[ -f $dir ]]; then
			continue
		fi
		complete=$(find $dir -name '*.txt' | wc -l)
		if [ $complete -eq $total ]; then
			continue
		fi
		echo "Progress for $(basename $dir)"
		echo -e "\t$complete / $total"
	done
}
export -f print_progress
watch -n 1200 -x bash -c "print_progress $1 $2"
