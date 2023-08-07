#!/bin/bash

io_files=("ipc" "mpki" "partial")

for f in ${io_files[@]}
do
	echo "Plotting ${f}.tsv"
	plotgen --debug -i ./${f}.tsv --print --palette bright --add-column AVG --apply-func add --apply-icolumns :12 13: --apply-func div 13 --apply-icolumns 13 --print --normalise-to-irow 3 --apply-func sub 1 --apply-icolumns : --x-type category --y-tick-format ',.2%' --print --plot bar --line-mode lines+markers --file ./raw_data/${output_name}.tsv -o ./graphs/${output_name}.html -o ./graphs/${output_name}.pdf
done
