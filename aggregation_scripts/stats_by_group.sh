#!/bin/bash

#benchmarks=("tanvir")
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")
#benchmarks=("crc2_spec" "crc2_cloud" "dpc3")
pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir="/cluster/home/romankb/plotgen/"; else pg_dir=$plotgen_dir; fi

echo ${pg_dir}


chroot=""

if [ -z ${champsim_root+x} ]; then chroot="/cluster/home/romankb"; else chroot=$champsim_root; fi

echo ${chroot}


for b in ${benchmarks[@]}
do
    echo "Accumulating ${b}"
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi MPKI &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi IPC  &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi PARTIAL &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi FRONTEND_STALLS &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi PARTIAL_MISSES &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi USELESS_LINES &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi BRANCH_MPKI &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi FETCH_COUNT &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi ROB_AT_MISS &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi STALL_CYCLES &
    python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b}/sizes_champsim32k single BRANCH_DISTANCES &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

for b in ${benchmarks[@]}
do
    echo "Plotting ${b}"

    echo "${pg_dir}plotgen --debug -i ./${b}/ipc.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative_${b}.tsv --width 1350 --height 300 -o ./graphs/ipc_relative_${b}.html ./graphs/ipc_relative_${b}.pdf"
    ${pg_dir}plotgen --debug -i ./${b}/mpki.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --reverse-columns --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/mpki_relative_${b}.tsv  --width 1350 --height 300 -o ./graphs/mpki_relative_${b}.html ./graphs/mpki_relative_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/ipc.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative_${b}.tsv  --width 1350 --height 300 -o ./graphs/ipc_relative_${b}.html ./graphs/ipc_relative_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/partial_relative_${b}.tsv  --width 1350 --height 300 -o ./graphs/partial_relative_${b}.html ./graphs/partial_relative_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/frontend_stalls.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_stalls_${b}.tsv  --width 1350 --height 300 -o ./graphs/frontend_stalls_${b}.html ./graphs/frontend_stalls_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/partial_misses_sizes_ubs_overhead_isca.tsv --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail_${b}.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 1350 --height 300 -o ./graphs/partial_detail_${b}.html ./graphs/partial_detail_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/partial_misses_sizes_champsim_vcl_buffer_fdip_extend_64d.tsv --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail_${b}.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 1350 --height 300 -o ./graphs/partial_detail_extend_${b}.html ./graphs/partial_detail_extend_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/useless.tsv --drop-nan --palette bright  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --file ./raw_data/useless_relative_${b}.tsv --width 1350 --height 300 -o ./graphs/useless_relative_${b}.html ./graphs/useless_relative_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/sizes_champsim_data_32k/**/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --add-function mean --add-column AVG --print --x-master-title "Useful Bytes in Cacheline" --y-master-title "% of Cachelines" --y-tick-format ',.2%' --palette bright --y-title-standoff 135 --file ./raw_data/accumulated_all_applications_${b}.tsv --width 1350 --height 300 -o ./graphs/accumulated_all_applications_${b}.html ./graphs/accumulated_all_applications_${b}.pdf  &

done

for job in `jobs -p`
do
    echo $job
    wait $job
done

python ${chroot}/ChampSim/aggregation_scripts/offset_plotting.py --result_dir ./ &

${pg_dir}plotgen --debug -i ./**/sizes_champsim_data_32k/**/cpu0_L1I_num_cl_with_num_holes.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --transpose --add-function sum --add-column "TOTAL CACHELINES" --sort-function name --sort-rows --row-names --select-column "TOTAL CACHELINES" --print --y-master-title "#Cachelines" --palette bright --master-title "Total Evictions" --plot bar --y-title-standoff 135 --file ./raw_data/cacheline_count.tsv --width 1350 --height 300 -o ./graphs/cacheline_count.html ./graphs/cacheline_count.pdf &
# TODO: fixup line and ipc script to generate headers ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/branch_distancesconst.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --transpose --add-function sum --add-column "TOTAL CACHELINES" --sort-function name --sort-rows --row-names --select-column "TOTAL CACHELINES" --print --y-master-title "#Cachelines" --palette bright --master-title "Total Evictions" --plot bar --y-title-standoff 135 --file ./raw_data/cacheline_count.tsv --width 1350 --height 300 -o ./graphs/cacheline_count.html ./graphs/cacheline_count.pdf &
${pg_dir}plotgen --debug -i ./**/ipc.tsv --drop-nan --palette bright --normalise-to-row $((2**15)) --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns :   --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative_overall.tsv --width 1350 --height 300 -o ./graphs/ipc_relative_overall.html ./graphs/ipc_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/mpki.tsv --drop-nan --palette bright --normalise-to-row $((2**15))  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/mpki_relative_overall.tsv --width 1350 --height 300 -o ./graphs/mpki_relative_overall.html ./graphs/mpki_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/flush_count.tsv --drop-nan --palette bright --normalise-to-row $((2**15))  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/flush_count_relative_overall.tsv --width 1350 --height 300 -o ./graphs/flush_count_relative_overall.html ./graphs/flush_count_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/fetch_count.tsv --drop-nan --palette bright --normalise-to-row $((2**15))  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/fetch_count_relative_overall.tsv --width 1350 --height 300 -o ./graphs/fetch_count_relative_overall.html ./graphs/fetch_count_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/frontend_stalls.tsv --drop-nan --palette bright --normalise-to-row $((2**15))  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_stalls_relative_overall.tsv --width 1350 --height 300 -o ./graphs/frontend_stalls_relative_overall.html ./graphs/frontend_stalls_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --column-names --filename --join columns --row-names --renameregex '(.*)\.champsimtrace' --select-column "UBS cache" --file ./raw_data/partial_relative_overall.tsv --width 625 --height 300 -o ./graphs/partial_relative_overall.html ./graphs/partial_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/**/cpu0_L1I_num_cl_with_num_holes.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --sort-function name --sort-columns --row-names --print --y-master-title "#Blocks" --palette bright --master-title "#Blocks / Cacheline" --plot bar --y-title-standoff 135 --select-irows 0:6 --transpose --file ./raw_data/block_per_cacheline_count.tsv --width 1350 --height 300 -o ./graphs/block_per_cacheline_count.html ./graphs/block_per_cacheline_count.pdf &
${pg_dir}plotgen --debug -i ./**/partial_misses_sizes_ubs_overhead_isca.tsv --join index --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail_overall.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 625 --height 300 -o ./graphs/partial_detail_overall.html ./graphs/partial_detail_overall.pdf &
${pg_dir}plotgen --debug -i ./**/partial_misses_sizes_champsim_vcl_buffer_fdip_extend_64d.tsv --join index --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --file ./raw_data/partial_detail_overall.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 625 --height 300 -o ./graphs/partial_detail_overall_extend.html ./graphs/partial_detail_overall_extend.pdf &
${pg_dir}plotgen --debug -i ./**/mpki.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/mpki_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/mpki_absolute_overall.html ./graphs/mpki_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/flush_count.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/flush_count_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/flush_count_absolute_overall.html ./graphs/flush_count_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/fetch_count.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/fetch_count_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/fetch_count_absolute_overall.html ./graphs/fetch_count_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/rob_at_miss.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/rob_at_miss_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/rob_at_miss_absolute_overall.html ./graphs/rob_at_miss_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/stall_cycles.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/stall_cycles_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/stall_cycles_absolute_overall.html ./graphs/stall_cycles_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/ipc.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/ipc_absolute_overall.html ./graphs/ipc_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/frontend_stalls.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/frontend_absolute_overall.html ./graphs/frontend_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/useless.tsv --drop-nan --join index --palette bright  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --row-names --renameregex '(.*)\.champsimtrace' --add-function mean --add-row AVG --file ./raw_data/useless_relative_overall.tsv --width 1350 --height 300 -o ./graphs/useless_relative_overall.html ./graphs/useless_relative_overall.pdf &

i=1
end=64

while [ $i -le $end ]; do
    ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/**/cpu0_size${i}_offset_count.tsv --drop-nan --palette bright --sort-order desc --sort-function median --sort-rows --drop-index --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*'      --join index --sort-function name --sort-columns --row-names --apply-function cset = nan 0 --apply-icolumns : --print --normalise-function sum --normalise-icolumns : --apply-fun cumsum --apply-columns : --apply-function cset = nan 1 --apply-icolumns : --drop-index --print --y-master-title "Relative Frequency of Offsets" --palette bright --master-title "" --plot line --y-title-standoff 135 --file ./raw_data/ordered_offset_${i}_counts.tsv --width 1350 --height 300 -o ./graphs/ordered_offset_${i}_counts.html ./graphs/ordered_offset_${i}_counts.pdf
    i=$(($i+1))
    #    ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/**/cpu0_size${i}_pc_offset_mapping.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --sort-function name --sort-columns --row-names --print --normalise-function sum --normalise-irows : --y-master-title "Relative Frequency of Offsets" --palette bright --master-title "" --plot line --y-title-standoff 135 --file ./raw_data/ordered_offset_${i}_counts.tsv --width 1350 --height 300 -o ./graphs/ordered_offset_${i}_counts.html ./graphs/ordered_offset_${i}_counts.pdf &
done


for job in `jobs -p`
    do
        echo $job
        wait $job
done

echo "DONE"
