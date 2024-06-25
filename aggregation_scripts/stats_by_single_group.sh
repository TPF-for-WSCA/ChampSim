#!/bin/bash

pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir="/cluster/home/romankb/plotgen/"; else pg_dir=$plotgen_dir; fi

echo ${pg_dir}


chroot=""

if [ -z ${champsim_root+x} ]; then chroot="/cluster/home/romankb"; else chroot=$champsim_root; fi

echo ${chroot}

	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./raw_logs/ multi MPKI
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./raw_logs/ multi IPC
	python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./raw_logs/ multi PARTIAL
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./raw_logs/ multi FRONTEND_STALLS
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./raw_logs/ multi PARTIAL_MISSES
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./raw_logs/ multi USELESS_LINES
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./raw_logs/sizes_champsim32k single BRANCH_DISTANCES

    
    echo "${pg_dir}plotgen --debug -i ./ipc.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative.tsv --width 1350 --height 300 -o ./graphs/ipc_relative.html ./graphs/ipc_relative.pdf"
    ${pg_dir}plotgen --debug -i ./raw_logs/mpki.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --reverse-columns --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/mpki_relative.tsv  --width 1350 --height 300 -o ./graphs/mpki_relative.html ./graphs/mpki_relative.pdf 
    ${pg_dir}plotgen --debug -i ./raw_logs/ipc.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative.tsv  --width 1350 --height 300 -o ./graphs/ipc_relative.html ./graphs/ipc_relative.pdf 
    ${pg_dir}plotgen --debug -i ./raw_logs/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/partial_relative.tsv  --width 1350 --height 300 -o ./graphs/partial_relative.html ./graphs/partial_relative.pdf 
    ${pg_dir}plotgen --debug -i ./raw_logs/frontend_stalls.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_stalls.tsv  --width 1350 --height 300 -o ./graphs/frontend_stalls.html ./graphs/frontend_stalls.pdf 
    ${pg_dir}plotgen --debug -i ./raw_logs/partial_misses_sizes_champsim_vcl_buffer_fdip_64d.tsv --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 1350 --height 300 -o ./graphs/partial_detail.html ./graphs/partial_detail.pdf 
    ${pg_dir}plotgen --debug -i ./raw_logs/partial_misses_sizes_champsim_vcl_buffer_fdip_extend_64d.tsv --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 1350 --height 300 -o ./graphs/partial_detail_extend.html ./graphs/partial_detail_extend.pdf 
    ${pg_dir}plotgen --debug -i ./raw_logs/useless.tsv --drop-nan --palette bright  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --file ./raw_data/useless_relative.tsv --width 1350 --height 300 -o ./graphs/useless_relative.html ./graphs/useless_relative.pdf 
    ${pg_dir}plotgen --debug -i ./raw_logs/sizes_champsim32k/**/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --add-function mean --add-column AVG --print --x-master-title "Useful Bytes in Cacheline" --y-master-title "% of Cachelines" --y-tick-format ',.2%' --palette bright --y-title-standoff 135 --file ./raw_data/accumulated_all_applications.tsv --width 1350 --height 300 -o ./graphs/accumulated_all_applications.html ./graphs/accumulated_all_applications.pdf 

    ${pg_dir}plotgen --debug -i ./**/sizes_champsim_data_32k/**/cpu0_L1I_num_cl_with_num_holes.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --transpose --add-function sum --add-column "TOTAL CACHELINES" --sort-function name --sort-rows --row-names --select-column "TOTAL CACHELINES" --print --y-master-title "#Cachelines" --palette bright --master-title "Total Evictions" --plot bar --y-title-standoff 135 --file ./raw_data/cacheline_count.tsv --width 1350 --height 300 -o ./graphs/cacheline_count.html ./graphs/cacheline_count.pdf
# TODO: fixup line and ipc script to generate headers ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/branch_distancesconst.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --transpose --add-function sum --add-column "TOTAL CACHELINES" --sort-function name --sort-rows --row-names --select-column "TOTAL CACHELINES" --print --y-master-title "#Cachelines" --palette bright --master-title "Total Evictions" --plot bar --y-title-standoff 135 --file ./raw_data/cacheline_count.tsv --width 1350 --height 300 -o ./graphs/cacheline_count.html ./graphs/cacheline_count.pdf
${pg_dir}plotgen --debug -i ./**/ipc.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns :   --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative_overall.tsv --width 1350 --height 300 -o ./graphs/ipc_relative_overall.html ./graphs/ipc_relative_overall.pdf
${pg_dir}plotgen --debug -i ./**/mpki.tsv --drop-nan --palette bright --normalise-to-row $((2**15))  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/mpki_relative_overall.tsv --width 1350 --height 300 -o ./graphs/mpki_relative_overall.html ./graphs/mpki_relative_overall.pdf
${pg_dir}plotgen --debug -i ./**/frontend_stalls.tsv --drop-nan --palette bright --normalise-to-row $((2**15))  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_stalls_relative_overall.tsv --width 1350 --height 300 -o ./graphs/frontend_stalls_relative_overall.html ./graphs/frontend_stalls_relative_overall.pdf
${pg_dir}plotgen --debug -i ./**/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --column-names --filename --join columns --row-names --renameregex '(.*)\.champsimtrace' --select-column "UBS cache" --file ./raw_data/partial_relative_overall.tsv --width 625 --height 300 -o ./graphs/partial_relative_overall.html ./graphs/partial_relative_overall.pdf
${pg_dir}plotgen --debug -i ./**/sizes_champsim_data_32k/**/cpu0_L1I_num_cl_with_num_holes.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --sort-function name --sort-columns --row-names --print --y-master-title "#Blocks" --palette bright --master-title "#Blocks / Cacheline" --plot bar --y-title-standoff 135 --select-irows 0:6 --transpose --file ./raw_data/block_per_cacheline_count.tsv --width 1350 --height 300 -o ./graphs/block_per_cacheline_count.html ./graphs/block_per_cacheline_count.pdf
${pg_dir}plotgen --debug -i ./**/partial_misses_sizes_champsim_vcl_buffer_fdip_64d.tsv --join index --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail_overall.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 625 --height 300 -o ./graphs/partial_detail_overall.html ./graphs/partial_detail_overall.pdf
${pg_dir}plotgen --debug -i ./**/partial_misses_sizes_champsim_vcl_buffer_fdip_extend_64d.tsv --join index --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail_overall.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 625 --height 300 -o ./graphs/partial_detail_overall_extend.html ./graphs/partial_detail_overall_extend.pdf
${pg_dir}plotgen --debug -i ./**/mpki.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/mpki_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/mpki_absolute_overall.html ./graphs/mpki_absolute_overall.pdf
${pg_dir}plotgen --debug -i ./**/frontend_stalls.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/frontend_absolute_overall.html ./graphs/frontend_absolute_overall.pdf
${pg_dir}plotgen --debug -i ./**/useless.tsv --drop-nan --join index --palette bright  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --file ./raw_data/useless_relative_overall.tsv --width 1350 --height 300 -o ./graphs/useless_relative_overall.html ./graphs/useless_relative_overall.pdf

echo "DONE"
