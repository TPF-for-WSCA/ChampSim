#!/bin/bash

#benchmarks=("tanvir")
benchmarks=("tanvir" "ipc1_server" "ipc1_client")
#benchmarks=("crc2_spec" "crc2_cloud")
pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir=""; else pg_dir=$plotgen_dir; fi

echo ${pg_dir}


chroot=""

if [ -z ${champsim_root+x} ]; then chroot=""; else chroot=$champsim_root; fi

echo ${chroot}

for b in ${benchmarks[@]}
do
	echo "Accumulating ${b}"
    cd ./${b}
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./ multi MPKI
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./ multi IPC
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./ multi PARTIAL
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./ multi FRONTEND_STALLS
    cd ..

	echo "Plotting ${b}"
    
    ${pg_dir}plotgen --debug -i ./${b}/mpki.tsv --drop-nan --palette bright --add-column AVG --apply-func add --apply-icolumns :-1 --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --file ./raw_data/mpki_relative_${b}.tsv -o ./graphs/mpki_relative_${b}.html
    ${pg_dir}plotgen --debug -i ./${b}/ipc.tsv --drop-nan --palette bright --add-column AVG --apply-func add --apply-icolumns :-1 --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --file ./raw_data/ipc_relative_${b}.tsv -o ./graphs/ipc_relative_${b}.html
    ${pg_dir}plotgen --debug -i ./${b}/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --file ./raw_data/partial_relative_${b}.tsv -o ./graphs/partial_relative_${b}.html
    ${pg_dir}plotgen --debug -i ./${b}/frontend_stalls.tsv --drop-nan --palette bright --add-column AVG --apply-func add --apply-icolumns :-1 --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --file ./raw_data/frontend_misses_${b}.tsv -o ./graphs/frontend_misses_${b}.html
done

echo "DONE"
