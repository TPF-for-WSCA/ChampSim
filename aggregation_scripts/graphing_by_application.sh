#!/bin/bash

tanvir_applications=("tomcat" "drupal" "verilator" "python" "postgres" "mysql" "finagle-http" "kafka" "mediawiki" "wordpress" "cassandra" "clang" "finagle-chirper")
ipc1_spec_app=("spec_x264_001.champsimtrace" "spec_gobmk_001.champsimtrace" "spec_gcc_003.champsimtrace" "spec_gobmk_002.champsimtrace" "spec_perlbench_001.champsimtrace" "spec_gcc_002.champsimtrace" "spec_gcc_001.champsimtrace")
ipc1_server_app=("server_012.champsimtrace" "server_028.champsimtrace" "server_027.champsimtrace" "server_031.champsimtrace" "server_025.champsimtrace" "server_036.champsimtrace" "server_020.champsimtrace" "server_022.champsimtrace" "server_018.champsimtrace" "server_013.champsimtrace" "server_011.champsimtrace" "server_039.champsimtrace" "server_024.champsimtrace" "server_035.champsimtrace" "server_033.champsimtrace" "server_015.champsimtrace" "server_021.champsimtrace" "server_026.champsimtrace" "server_003.champsimtrace" "server_001.champsimtrace" "server_032.champsimtrace" "server_009.champsimtrace" "server_029.champsimtrace" "server_014.champsimtrace" "server_019.champsimtrace" "server_037.champsimtrace" "server_016.champsimtrace" "server_023.champsimtrace" "server_004.champsimtrace" "server_030.champsimtrace" "server_002.champsimtrace" "server_017.champsimtrace" "server_038.champsimtrace" "server_010.champsimtrace" "server_034.champsimtrace")
ipc1_client_app=("client_008.champsimtrace" "client_005.champsimtrace" "client_004.champsimtrace" "client_003.champsimtrace" "client_006.champsimtrace" "client_001.champsimtrace" "client_002.champsimtrace" "client_007.champsimtrace")

pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir=""; else pg_dir=$plotgen_dir; fi

echo ${pg_dir}

# Tanvir Workloads
for application in ${tanvir_applications[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./tanvir/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html
done

# IPC 1 Workload
for application in ${ipc1_spec_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./ipc1_spec/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filenameframes .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
done
for application in ${ipc1_server_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./ipc1_server/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
done
for application in ${ipc1_client_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./ipc1_client/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
done

echo "DONE"
