#!/bin/bash

#benchmarks=("tanvir")
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")
#benchmarks=("crc2_spec" "crc2_cloud" "dpc3")
pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir=""; else pg_dir=$plotgen_dir; fi

echo ${pg_dir}


chroot=""

if [ -z ${champsim_root+x} ]; then chroot=""; else chroot=$champsim_root; fi

echo ${chroot}

for b in ${benchmarks[@]}
do
	echo "Accumulating ${b}"
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi MPKI
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi IPC
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi PARTIAL
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi FRONTEND_STALLS
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi PARTIAL_MISSES
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi USELESS_LINES

	echo "Plotting ${b}"
    
    ${pg_dir}plotgen --debug -i ./${b}/mpki.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buﬀer_64d_data --add-function mean --add-column AVG --file ./raw_data/mpki_relative_${b}.tsv -o ./graphs/mpki_relative_${b}.html --width 1800 --height 400 -o ./graphs/mpki_relative_${b}.pdf 
    ${pg_dir}plotgen --debug -i ./${b}/ipc.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buﬀer_64d_data --add-function mean --add-column AVG --file ./raw_data/ipc_relative_${b}.tsv -o ./graphs/ipc_relative_${b}.html --width 1800 --height 400 -o ./graphs/ipc_relative_${b}.pdf 
    ${pg_dir}plotgen --debug -i ./${b}/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buﬀer_64d_data --file ./raw_data/partial_relative_${b}.tsv -o ./graphs/partial_relative_${b}.html --width 1800 --height 400 -o ./graphs/partial_relative_${b}.pdf 
    ${pg_dir}plotgen --debug -i ./${b}/frontend_stalls.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buﬀer_64d_data --add-function mean --add-column AVG --file ./raw_data/frontend_stalls_${b}.tsv -o ./graphs/frontend_stalls_${b}.html --width 1800 --height 400 -o ./graphs/frontend_stalls_${b}.pdf 
    ${pg_dir}plotgen --debug -i ./${b}/partial_misses.tsv --drop-nan --group-by-irows 0 --palette bright --x-type category --plot bar --file ./raw_data/partial_detail_${b}.tsv -o ./graphs/partial_detail_${b}.html --width 1800 --height 400 -o ./graphs/partial_detail_${b}.pdf 
    ${pg_dir}plotgen --debug -i ./${b}/useless.tsv --drop-nan --palette bright  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buﬀer_64d_data --add-function mean --add-column AVG --file ./raw_data/useless_relative_${b}.tsv -o ./graphs/useless_relative_${b}.html --width 1800 --height 400 -o ./graphs/useless_relative_${b}.pdf 
done

echo "DONE"
