#!/bin/bash

benchmarks=("tanvir", "ipc1_spec", "ipc1_server", "ipc1_client")

for b in ${benchmarks[@]}
do
	echo "Accumulating ${b}"
	python ~/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi MPKI
	python ~/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi IPC
	python ~/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi PARTIAL

	echo "Plotting ${b}"
	plotgen -i mpki.tsv --palette bright --add-column AVG --apply-func add --apply-icolumns : 
done
