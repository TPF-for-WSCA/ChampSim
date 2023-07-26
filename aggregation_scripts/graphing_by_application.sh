#!/bin/bash

tanvir_applications=("tomcat" "drupal" "verilator" "python" "postgres" "mysql" "finagle-http" "kafka" "mediawiki" "wordpress" "cassandra" "clang" "finagle-chirper")
#ipc1_spec_app=("spec_x264_001.champsimtrace" "spec_gobmk_001.champsimtrace" "spec_gcc_003.champsimtrace" "spec_gobmk_002.champsimtrace" "spec_perlbench_001.champsimtrace" "spec_gcc_002.champsimtrace" "spec_gcc_001.champsimtrace")
ipc1_server_app=("server_012.champsimtrace" "server_028.champsimtrace" "server_027.champsimtrace" "server_031.champsimtrace" "server_025.champsimtrace" "server_036.champsimtrace" "server_020.champsimtrace" "server_022.champsimtrace" "server_018.champsimtrace" "server_013.champsimtrace" "server_011.champsimtrace" "server_039.champsimtrace" "server_024.champsimtrace" "server_035.champsimtrace" "server_033.champsimtrace" "server_015.champsimtrace" "server_021.champsimtrace" "server_026.champsimtrace" "server_003.champsimtrace" "server_001.champsimtrace" "server_032.champsimtrace" "server_009.champsimtrace" "server_029.champsimtrace" "server_014.champsimtrace" "server_019.champsimtrace" "server_037.champsimtrace" "server_016.champsimtrace" "server_023.champsimtrace" "server_004.champsimtrace" "server_030.champsimtrace" "server_002.champsimtrace" "server_017.champsimtrace" "server_038.champsimtrace" "server_010.champsimtrace" "server_034.champsimtrace")
ipc1_client_app=("client_008.champsimtrace" "client_005.champsimtrace" "client_004.champsimtrace" "client_003.champsimtrace" "client_006.champsimtrace" "client_001.champsimtrace" "client_002.champsimtrace" "client_007.champsimtrace")
#crc2_spec_app=("tonto_2834B.trace" "sjeng_358B.trace" "tonto_422B.trace" "gobmk_135B.trace" "bzip2_259B.trace" "gamess_247B.trace" "xalancbmk_768B.trace" "wrf_1228B.trace" "h264ref_273B.trace" "xalancbmk_99B.trace" "omnetpp_4B.trace" "namd_1907B.trace" "omnetpp_340B.trace" "povray_437B.trace" "calculix_3812B.trace" "gamess_196B.trace" "gcc_56B.trace" "gobmk_76B.trace" "wrf_1650B.trace" "perlbench_105B.trace" "cactusADM_734B.trace" "milc_409B.trace" "cactusADM_1039B.trace" "sphinx3_1339B.trace" "GemsFDTD_716B.trace" "gobmk_60B.trace" "xalancbmk_748B.trace" "h264ref_178B.trace" "bzip2_281B.trace" "sjeng_1109B.trace" "astar_23B.trace" "calculix_2655B.trace" "namd_400B.trace" "gromacs_0B.trace" "gcc_13B.trace" "sphinx3_883B.trace" "GemsFDTD_712B.trace" "soplex_217B.trace" "bwaves_98B.trace" "bwaves_1609B.trace" "zeusmp_300B.trace" "sphinx3_2520B.trace" "libquantum_964B.trace" "wrf_1212B.trace" "perlbench_135B.trace" "zeusmp_100B.trace" "gcc_39B.trace" "libquantum_1210B.trace" "hmmer_546B.trace" "povray_711B.trace" "leslie3d_1186B.trace" "zeusmp_600B.trace" "astar_313B.trace" "tonto_2049B.trace" "soplex_66B.trace" "bzip2_183B.trace" "calculix_2670B.trace" "h264ref_351B.trace" "gamess_316B.trace" "astar_163B.trace" "lbm_94B.trace" "hmmer_397B.trace" "omnetpp_17B.trace" "lbm_564B.trace" "cactusADM_1495B.trace" "libquantum_1735B.trace" "milc_744B.trace" "povray_250B.trace" "sjeng_1966B.trace" "gromacs_1B.trace" "namd_851B.trace" "mcf_46B.trace" "perlbench_53B.trace" "bwaves_1861B.trace" "leslie3d_94B.trace" "milc_360B.trace" "soplex_205B.trace" "leslie3d_1116B.trace" "mcf_158B.trace" "hmmer_7B.trace" "mcf_250B.trace" "lbm_1004B.trace" "GemsFDTD_109B.trace") 
#crc2_cloud_app=("streaming_phase4_core1.trace" "streaming_phase4_core0.trace" "streaming_phase0_core2.trace" "nutch_phase5_core3.trace" "nutch_phase0_core3.trace" "classification_phase2_core3.trace" "classification_phase0_core1.trace" "cassandra_phase1_core3.trace" "streaming_phase5_core3.trace" "streaming_phase1_core2.trace" "nutch_phase3_core3.trace" "nutch_phase3_core0.trace" "nutch_phase1_core3.trace" "cloud9_phase5_core2.trace" "cloud9_phase4_core1.trace" "classification_phase3_core1.trace" "cassandra_phase5_core2.trace" "cassandra_phase5_core0.trace" "cassandra_phase3_core1.trace" "cassandra_phase1_core2.trace" "cassandra_phase0_core2.trace" "streaming_phase4_core3.trace" "nutch_phase0_core1.trace" "cloud9_phase4_core0.trace" "classification_phase4_core3.trace" "classification_phase2_core1.trace" "nutch_phase4_core3.trace" "cloud9_phase2_core2.trace" "classification_phase5_core2.trace" "cassandra_phase3_core0.trace" "nutch_phase4_core1.trace" "cloud9_phase0_core1.trace" "classification_phase3_core0.trace" "cassandra_phase1_core1.trace" "streaming_phase0_core0.trace" "nutch_phase3_core1.trace" "cloud9_phase5_core3.trace" "classification_phase5_core0.trace" "classification_phase4_core0.trace" "streaming_phase5_core1.trace" "streaming_phase3_core0.trace" "cassandra_phase2_core2.trace" "streaming_phase1_core0.trace" "cloud9_phase3_core0.trace" "cloud9_phase4_core3.trace" "cassandra_phase2_core3.trace" "nutch_phase4_core0.trace" "cassandra_phase2_core0.trace" "nutch_phase5_core2.trace" "nutch_phase0_core0.trace" "cassandra_phase3_core2.trace" "streaming_phase3_core2.trace" "cloud9_phase3_core3.trace" "cloud9_phase3_core1.trace" "cloud9_phase2_core1.trace" "cloud9_phase0_core0.trace" "cassandra_phase4_core3.trace" "streaming_phase0_core1.trace" "nutch_phase1_core1.trace" "classification_phase3_core3.trace" "classification_phase1_core0.trace" "classification_phase0_core0.trace" "cassandra_phase5_core3.trace" "nutch_phase3_core2.trace" "classification_phase1_core3.trace" "streaming_phase3_core3.trace" "cloud9_phase2_core0.trace" "classification_phase5_core3.trace" "cassandra_phase4_core2.trace" "streaming_phase2_core0.trace" "classification_phase2_core0.trace" "classification_phase2_core2.trace" "cassandra_phase4_core1.trace" "streaming_phase3_core1.trace" "cloud9_phase4_core2.trace" "nutch_phase4_core2.trace" "nutch_phase1_core2.trace" "streaming_phase5_core2.trace" "cloud9_phase5_core0.trace" "cloud9_phase1_core2.trace" "cassandra_phase4_core0.trace" "cassandra_phase3_core3.trace" "cassandra_phase0_core3.trace" "cassandra_phase0_core1.trace" "streaming_phase1_core1.trace" "cloud9_phase3_core2.trace" "cloud9_phase1_core3.trace" "cassandra_phase5_core1.trace" "streaming_phase0_core3.trace" "classification_phase0_core2.trace" "cassandra_phase1_core0.trace" "streaming_phase2_core1.trace" "streaming_phase1_core3.trace" "classification_phase5_core1.trace" "classification_phase4_core1.trace" "streaming_phase5_core0.trace" "streaming_phase2_core2.trace" "cassandra_phase2_core1.trace" "cassandra_phase0_core0.trace" "nutch_phase0_core2.trace" "cloud9_phase5_core1.trace" "classification_phase3_core2.trace" "streaming_phase2_core3.trace" "classification_phase1_core2.trace" "streaming_phase4_core2.trace" "nutch_phase1_core0.trace" "classification_phase1_core1.trace" "classification_phase0_core3.trace" "cloud9_phase1_core0.trace" "cloud9_phase1_core1.trace" "nutch_phase2_core0.trace" "nutch_phase5_core0.trace" "classification_phase4_core2.trace" "nutch_phase5_core1.trace" "cloud9_phase2_core3.trace" "nutch_phase2_core1.trace" "nutch_phase2_core3.trace" "cloud9_phase0_core2.trace" "nutch_phase2_core2.trace" "cloud9_phase0_core3.trace")

pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir=""; else pg_dir=$plotgen_dir; fi

echo ${pg_dir}

# Tanvir Workloads
for application in ${tanvir_applications[@]}
do
    echo "${pg_dir}plotgen --debug -i ./tanvir/sizes_*champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html
done

# IPC 1 Workload
for application in ${ipc1_spec_app[@]}
do
    echo "${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_*champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filenameframes .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filenameframes .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html
done
for application in ${ipc1_server_app[@]}
do
    echo "${pg_dir}plotgen --debug -i ./ipc1_server/sizes_*champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html
done
for application in ${ipc1_client_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./ipc1_client/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html
done

# CRC2
for application in ${crc2_cloud_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./crc2_cloud/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html
done
for application in ${crc2_spec_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./crc2_spec/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html"
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html
done

echo "DONE"
