#!/bin/bash

#benchmarks=("tanvir")
#inputs: 
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec" "LLBP" "dpc3" "google_merced" "google_charlie" "google_delta" "google_whiskey") #  "LLBP")
normalise_to_row="sizes_btb_tag_full"  # TODO: Make this the default/add default to look at / baseline
#end inputs~
#benchmarks=("crc2_spec" "crc2_cloud" "dpc3")
pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir="/cluster/projects/nn4650k/workspace/plotgen/"; else pg_dir=$plotgen_dir; fi

echo "pg_dir: ${pg_dir}"


chroot=""

if [ -z ${champsim_root+x} ]; then chroot="/cluster/projects/nn4650k/workspace"; else chroot=$champsim_root; fi

echo "chroot: ${chroot}"

mkdir -p raw_data
mkdir -p graphs
mkdir -p raw_data/btb_tag_bits

if [ $# -lt 1 ]; then
    for b in ${benchmarks[@]}
    do
        echo "Accumulating ${b}"
        python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi MPKI &
        python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi IPC  &
        python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi BTB_ALIASING  &
        python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi BTB_TOTAL_ALIASING  &
        python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi BTB_TAG_ENTROPY  &
        python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi BTB_BIT_ORDERING  &

        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi PARTIAL &
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi FRONTEND_STALLS &
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi PARTIAL_MISSES &
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi USELESS_LINES &
        python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi BRANCH_MPKI &
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi FETCH_COUNT &
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi ROB_AT_MISS &
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b} multi STALL_CYCLES &
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b}/sizes_btb_tag_full single BRANCH_DISTANCES &
        # for config in ./${b}/*;
        # do
        #     echo "Extract ${config}"
        #     python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ${config} single BTB_ALIASING &
        # done
        # python ${chroot}/ChampSim/aggregation_scripts/ipc_data.py ./${b}/sizes_btbx_full_tag single BTB_BIT_INFORMATION &
    done
else echo "skip aggregation";
fi

# Collect all btb bit information files
find $(find . -name "*full*") -name "*bit_ordering.txt" -exec cp {} ./raw_data/btb_tag_bits ';'
pushd raw_data/btb_tag_bits
echo ./*.txt | sed 's/ /\t/g' > header.txt
paste ./*bit_ordering.txt > combined_bit_ordering.csv

popd

# for b in ${benchmarks[@]}
# do
#     for application in ./$b/sizes_btb_tag_full/*;
#     do
#         for ((i=1;i<=64;i++)); do
#             echo "Plotting ${application}"
#             ${pg_dir}plotgen --debug -i $application/cpu0_partition${i}_dynamic_offset_count.tsv -i $application/cpu0_partition${i}_dynamic_branch_count.tsv -i $application/cpu0_partition${i}_dynamic_target_count.tsv &
#         done
#     done
# done

echo "Waiting for jobs to finish:"
for job in `jobs -p`
do
    echo $job
    wait $job
done

echo "aggregation scripts finished"

for b in ${benchmarks[@]}
do
    echo "Plotting ${b}"

    echo "${pg_dir}plotgen --debug -i ./${b}/ipc.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row} --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative_${b}.tsv --width 1350 --height 300 -o ./graphs/ipc_relative_${b}.html ./graphs/ipc_relative_${b}.pdf"
    ${pg_dir}plotgen --debug -i ./${b}/ipc.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row} --apply-func cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative_${b}.tsv  --width 1350 --height 300 -o ./graphs/ipc_relative_${b}.html ./graphs/ipc_relative_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/branch_mpki.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row} --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-columns --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --reverse-columns --row-names --renameregex 'uuujq' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/branch_mpki_relative_${b}.tsv  --width 1350 --height 300 -o ./graphs/branch_mpki_relative_${b}.html ./graphs/branch_mpki_relative_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/aliasing.tsv --drop-nan --palette bright --apply-function cset = nan 1 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-columns --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/aliasing_relative_${b}.tsv  --width 1350 --height 300 -o ./graphs/alias_relative_${b}.html ./graphs/alias_relative_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/total_aliasing.tsv --drop-nan --palette bright --apply-function cset = nan 1 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-columns --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/aliasing_absolute_${b}.tsv  --width 1350 --height 300 -o ./graphs/alias_absolute_${b}.html ./graphs/alias_absolute_${b}.pdf  &
    ${pg_dir}plotgen --debug -i ./${b}/btb_tag_entropy.tsv --drop-nan --palette bright --apply-function cset = nan 1 --apply-icolumns : --x-type category --plot bar --transpose --sort-function name --sort-columns --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/entropy_${b}.tsv  --width 1350 --height 300 -o ./graphs/entropy_${b}.html ./graphs/entropy_${b}.pdf  &
    # ${pg_dir}plotgen --debug -i ./${b}/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/partial_relative_${b}.tsv  --width 1350 --height 300 -o ./graphs/partial_relative_${b}.html ./graphs/partial_relative_${b}.pdf  &
    # ${pg_dir}plotgen --debug -i ./${b}/frontend_stalls.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row} --apply-func sub 1 --apply-icolumns : --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-by-column sizes_champsim_vcl_buffer_fdip_64d --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_stalls_${b}.tsv  --width 1350 --height 300 -o ./graphs/frontend_stalls_${b}.html ./graphs/frontend_stalls_${b}.pdf  &
    # ${pg_dir}plotgen --debug -i ./${b}/partial_misses_sizes_ubs_overhead_isca.tsv --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --file ./raw_data/partial_detail_${b}.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 1350 --height 300 -o ./graphs/partial_detail_${b}.html ./graphs/partial_detail_${b}.pdf  &
    # ${pg_dir}plotgen --debug -i ./${b}/partial_misses_sizes_champsim_vcl_buffer_fdip_extend_64d.tsv --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --file ./raw_data/partial_detail_${b}.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 1350 --height 300 -o ./graphs/partial_detail_extend_${b}.html ./graphs/partial_detail_extend_${b}.pdf  &
    # ${pg_dir}plotgen --debug -i ./${b}/useless.tsv --drop-nan --palette bright  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/useless_relative_${b}.tsv --width 1350 --height 300 -o ./graphs/useless_relative_${b}.html ./graphs/useless_relative_${b}.pdf  &
    # ${pg_dir}plotgen --debug -i ./${b}/sizes_champsim_data_32k/**/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --column-names --renameregex '\./(.*)/.*/([a-zA-Z\-_0-9\.]+)/\.*' --join index --add-function mean --add-column AVG --print --x-master-title "Useful Bytes in Cacheline" --y-master-title "% of Cachelines" --y-tick-format ',.2%' --palette bright --y-title-standoff 135 --file ./raw_data/accumulated_all_applications_${b}.tsv --width 1350 --height 300 -o ./graphs/accumulated_all_applications_${b}.html ./graphs/accumulated_all_applications_${b}.pdf  &

done

echo "Waiting for jobs to finish:"
for job in `jobs -p`
do
    echo $job
    wait $job
done
echo "aggregation scripts finished"

python ${chroot}/ChampSim/aggregation_scripts/offset_plotting.py --result_dir ./ &
for ((i=1;i<=64;i++)); do
    ${pg_dir}plotgen -i ./**/**/**/cpu0_partition${i}_dynamic_offset_count.tsv -i ./**/**/**/cpu0_partition${i}_dynamic_branch_count.tsv -i ./**/**/**/cpu0_partition${i}_dynamic_target_count.tsv --no-columns  --column-names --filename --column-names --renameregex '\./(.*)/(.*)/([a-zA-Z\-_0-9\.]+)/\.*' --normalise-function sum --normalise-columns : --join index --apply-function cset = nan 0 --apply-icolumns : --plot line --file ./raw_data/ordered_offset_partition_${i}.tsv  --palette bright -o ./graphs/ordered_offset_partition_${i}.html &
done
# ${pg_dir}plotgen --debug -i ./**/sizes_champsim_data_32k/**/cpu0_L1I_num_cl_with_num_holes.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --transpose --add-function sum --add-column "TOTAL CACHELINES" --sort-function name --sort-rows --row-names --select-column "TOTAL CACHELINES" --print --y-master-title "#Cachelines" --palette bright --master-title "Total Evictions" --plot bar --y-title-standoff 135 --file ./raw_data/cacheline_count.tsv --width 1350 --height 300 -o ./graphs/cacheline_count.html ./graphs/cacheline_count.pdf &
# TODO: fixup line and ipc script to generate headers ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/branch_distancesconst.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --transpose --add-function sum --add-column "TOTAL CACHELINES" --sort-function name --sort-rows --row-names --select-column "TOTAL CACHELINES" --print --y-master-title "#Cachelines" --palette bright --master-title "Total Evictions" --plot bar --y-title-standoff 135 --file ./raw_data/cacheline_count.tsv --width 1350 --height 300 -o ./graphs/cacheline_count.html ./graphs/cacheline_count.pdf &
${pg_dir}plotgen --debug -i ./**/ipc.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row} --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns :   --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_relative_overall.tsv --width 1350 --height 300 -o ./graphs/ipc_relative_overall.html ./graphs/ipc_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/branch_mpki.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row}  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/branch_mpki_relative_overall.tsv --width 1350 --height 300 -o ./graphs/branch_mpki_relative_overall.html ./graphs/branch_mpki_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/aliasing.tsv --drop-nan --palette bright --apply-function cset = nan 1 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/aliasing_relative_overall.tsv --width 1350 --height 300 -o ./graphs/aliasing_relative_overall.html ./graphs/aliasing_relative_overall.pdf &
${pg_dir}plotgen --debug -i ./**/total_aliasing.tsv --drop-nan --palette bright --apply-function cset = nan 1 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/aliasing_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/aliasing_absolute_overall.html ./graphs/aliasing_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/btb_tag_entropy.tsv --drop-nan --palette bright --apply-function cset = nan 1 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/entropy_overall.tsv --width 1350 --height 300 -o ./graphs/entropy_overall.html ./graphs/entropy_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/branch_mpki.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row}  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/flush_count_relative_overall.tsv --width 1350 --height 300 -o ./graphs/flush_count_relative_overall.html ./graphs/flush_count_relative_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/fetch_count.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row}  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/fetch_count_relative_overall.tsv --width 1350 --height 300 -o ./graphs/fetch_count_relative_overall.html ./graphs/fetch_count_relative_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/frontend_stalls.tsv --drop-nan --palette bright --normalise-to-row ${normalise_to_row}  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --apply-func sub 1 --apply-icolumns : --ignore-columns $((2**15)) --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_stalls_relative_overall.tsv --width 1350 --height 300 -o ./graphs/frontend_stalls_relative_overall.html ./graphs/frontend_stalls_relative_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/partial.tsv --drop-nan --apply-func div 100 --apply-icolumns : --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --column-names --filename --join columns --row-names --renameregex '(.*)\..*trace' --select-column "UBS cache" --file ./raw_data/partial_relative_overall.tsv --width 625 --height 300 -o ./graphs/partial_relative_overall.html ./graphs/partial_relative_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/**/cpu0_L1I_num_cl_with_num_holes.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --sort-function name --sort-columns --row-names --print --y-master-title "#Blocks" --palette bright --master-title "#Blocks / Cacheline" --plot bar --y-title-standoff 135 --select-irows 0:6 --transpose --file ./raw_data/block_per_cacheline_count.tsv --width 1350 --height 300 -o ./graphs/block_per_cacheline_count.html ./graphs/block_per_cacheline_count.pdf &
# ${pg_dir}plotgen --debug -i ./**/partial_misses_sizes_ubs_overhead_isca.tsv --join index --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --file ./raw_data/partial_detail_overall.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 625 --height 300 -o ./graphs/partial_detail_overall.html ./graphs/partial_detail_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/partial_misses_sizes_champsim_vcl_buffer_fdip_extend_64d.tsv --join index --drop-nan --transpose --normalise-function sum --normalise-irows : --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --file ./raw_data/partial_detail_overall.tsv --y-tick-format ',.2%'  --palette bright --x-type category --plot bar --width 625 --height 300 -o ./graphs/partial_detail_overall_extend.html ./graphs/partial_detail_overall_extend.pdf &
${pg_dir}plotgen --debug -i ./**/branch_mpki.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/branch_mpki_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/branch_mpki_absolute_overall.html ./graphs/branch_mpki_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/mpki.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/mpki_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/mpki_absolute_overall.html ./graphs/mpki_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/branch_mpki.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/flush_count_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/flush_count_absolute_overall.html ./graphs/flush_count_absolute_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/fetch_count.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/fetch_count_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/fetch_count_absolute_overall.html ./graphs/fetch_count_absolute_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/rob_at_miss.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/rob_at_miss_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/rob_at_miss_absolute_overall.html ./graphs/rob_at_miss_absolute_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/stall_cycles.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/stall_cycles_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/stall_cycles_absolute_overall.html ./graphs/stall_cycles_absolute_overall.pdf &
${pg_dir}plotgen --debug -i ./**/ipc.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-columns --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/ipc_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/ipc_absolute_overall.html ./graphs/ipc_absolute_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/frontend_stalls.tsv --drop-nan --palette bright --apply-function cset = nan 0 --apply-icolumns : --x-type category --plot bar --join index --transpose --sort-function name --sort-rows --row-names --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --column-names 'sizes_champsim_vcl_buffer_fdip_64d:UBS cache' --file ./raw_data/frontend_absolute_overall.tsv --width 1350 --height 300 -o ./graphs/frontend_absolute_overall.html ./graphs/frontend_absolute_overall.pdf &
# ${pg_dir}plotgen --debug -i ./**/useless.tsv --drop-nan --join index --palette bright  --apply-function cset = nan 0 --apply-icolumns : --x-type category --y-tick-format ',.2%' --plot bar --transpose --sort-function name --sort-rows --row-names --renameregex '(.*)\..*trace' --add-function mean --add-row AVG --file ./raw_data/useless_relative_overall.tsv --width 1350 --height 300 -o ./graphs/useless_relative_overall.html ./graphs/useless_relative_overall.pdf &

i=1
end=64

# TODO: Bring offsets back into current implementation analysis
# while [ $i -le $end ]; do
#     ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/**/cpu0_size${i}_offset_count.tsv --drop-nan --palette bright --sort-order desc --sort-function median --sort-rows --drop-index --column-names --filename --column-names --renameregex '\./(.*)/.*/([a-zA-Z\-_0-9\.]+)/\.*' --join index --sort-function name --sort-columns --row-names --apply-function cset = nan 0 --apply-icolumns : --print --normalise-function sum --normalise-icolumns : --apply-fun cumsum --apply-columns : --apply-function cset = nan 1 --apply-icolumns : --drop-index --print --y-master-title "Relative Frequency of Offsets" --palette bright --master-title "" --plot line --y-title-standoff 135 --file ./raw_data/ordered_offset_${i}_counts.tsv --width 1350 --height 300 -o ./graphs/ordered_offset_${i}_counts.html ./graphs/ordered_offset_${i}_counts.pdf &
#     i=$(($i+1))
#     #    ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/**/cpu0_size${i}_pc_offset_mapping.tsv --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*' --join index --sort-function name --sort-columns --row-names --print --normalise-function sum --normalise-irows : --y-master-title "Relative Frequency of Offsets" --palette bright --master-title "" --plot line --y-title-standoff 135 --file ./raw_data/ordered_offset_${i}_counts.tsv --width 1350 --height 300 -o ./graphs/ordered_offset_${i}_counts.html ./graphs/ordered_offset_${i}_counts.pdf &
# done
# 
# for ((i=1;i<=64;i++)); do
#     ${pg_dir}plotgen -i ./**/sizes_offset_btbx/**/cpu0_size${i}_offset_count.tsv --no-columns --column-names --filename --column-names --renameregex '\./(.*)/.*/([a-zA-Z\-_0-9\.]+)/\.*' --normalise-function sum --normalise-columns : --join index --apply-function cset = nan 0 --apply-icolumns : --sort-rows --plot line --file ./raw_data/ordered_offset_${i}.tsv  --palette bright -o ./graphs/ordered_offset_${i}.html &
# done
# # TODO: add a plotgen for the summary of the sizes
# ${pg_dir}plotgen --debug -i ./**/ordered_offset_counts.tsv --drop-nan --palette bright --sort-order desc --sort-function median --sort-rows --drop-index --column-names --filename --column-names --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.champsimtrace/\.*'      --join index --sort-function name --sort-columns --row-names --apply-function cset = nan 0 --apply-icolumns : --print --normalise-function sum --normalise-icolumns : --apply-fun cumsum --apply-columns : --apply-function cset = nan 1 --apply-icolumns : --drop-index --print --y-master-title "Relative Frequency of Offsets" --palette bright --master-title "" --plot line --y-title-standoff 135 --file ./raw_data/ordered_offset_overall_counts.tsv --width 1350 --height 300 -o ./graphs/ordered_offset_overall_counts.html ./graphs/ordered_offset_overall_counts.pdf &
# 
# ${pg_dir}plotgen --debug -i ./**/sizes_champsim32k/**/cpu0_num_bits_count.tsv --drop-nan --palette bright --drop-index --print --column-names --filename --column-names --renameregex '\./(.*)/.*/([a-zA-Z\-_0-9\.]+)/\.*' --print --join index --sort-function name --sort-columns --row-names --apply-function cset = nan 0 --apply-icolumns : --print --file ./raw_data/offset_sizes.tsv --normalise-function sum --normalise-icolumns : --apply-fun cumsum --apply-columns : --apply-function cset = nan 1 --apply-icolumns : --drop-index --print --y-master-title "Fraction of dynamic branches covered" --x-master-title "Number of bits required for branch target offsets" --palette bright --master-title "Distribution of branch target offsets in different workloads" --plot line --y-title-standoff 135 --file ./raw_data/offset_size_accumulated.tsv --width 1350 --height 300 -o ./graphs/offset_size_accumulated.html ./graphs/offset_size_accumulated.pdf &

for job in `jobs -p`
    do
        echo $job
        wait $job
done

echo "DONE"


# Usefule things:
# aliasing plot: ~/plotgen/plotgen -i ./aliasing.tsv --sort-function name --sort-rows --drop-nan --normalise-to-column "Total Lookups" --y-tick-format ',.2%' --plot bar -o ../graphs/btb_16b_aliasing.html
# address heatmap:  ~/plotgen/plotgen -i ./aggregated/btb_bit_information.tsv --palette bright --drop-nan --plot heatmap -o ./graphs/btb_address_heatmap.html
