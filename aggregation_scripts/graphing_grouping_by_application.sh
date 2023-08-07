#!/bin/bash

#tanvir_applications=("tomcat" "drupal" "verilator" "python" "postgres" "mysql" "finagle-http" "kafka" "mediawiki" "wordpress" "cassandra" "clang" "finagle-chirper")
#ipc1_spec_app=("spec_x264_001.champsimtrace" "spec_gobmk_001.champsimtrace" "spec_gcc_003.champsimtrace" "spec_gobmk_002.champsimtrace" "spec_perlbench_001.champsimtrace" "spec_gcc_002.champsimtrace" "spec_gcc_001.champsimtrace")
ipc1_server_app=("server_012.champsimtrace" "server_028.champsimtrace" "server_027.champsimtrace" "server_031.champsimtrace" "server_025.champsimtrace" "server_036.champsimtrace" "server_020.champsimtrace" "server_022.champsimtrace" "server_018.champsimtrace" "server_013.champsimtrace" "server_011.champsimtrace" "server_039.champsimtrace" "server_024.champsimtrace" "server_035.champsimtrace" "server_033.champsimtrace" "server_015.champsimtrace" "server_021.champsimtrace" "server_026.champsimtrace" "server_003.champsimtrace" "server_001.champsimtrace" "server_032.champsimtrace" "server_009.champsimtrace" "server_029.champsimtrace" "server_014.champsimtrace" "server_019.champsimtrace" "server_037.champsimtrace" "server_016.champsimtrace" "server_023.champsimtrace" "server_004.champsimtrace" "server_030.champsimtrace" "server_002.champsimtrace" "server_017.champsimtrace" "server_038.champsimtrace" "server_010.champsimtrace" "server_034.champsimtrace")
ipc1_client_app=("client_008.champsimtrace" "client_005.champsimtrace" "client_004.champsimtrace" "client_003.champsimtrace" "client_006.champsimtrace" "client_001.champsimtrace" "client_002.champsimtrace" "client_007.champsimtrace")
#crc2_spec_app=("tonto_2834B.trace" "sjeng_358B.trace" "tonto_422B.trace" "gobmk_135B.trace" "bzip2_259B.trace" "gamess_247B.trace" "xalancbmk_768B.trace" "wrf_1228B.trace" "h264ref_273B.trace" "xalancbmk_99B.trace" "omnetpp_4B.trace" "namd_1907B.trace" "omnetpp_340B.trace" "povray_437B.trace" "calculix_3812B.trace" "gamess_196B.trace" "gcc_56B.trace" "gobmk_76B.trace" "wrf_1650B.trace" "perlbench_105B.trace" "cactusADM_734B.trace" "milc_409B.trace" "cactusADM_1039B.trace" "sphinx3_1339B.trace" "GemsFDTD_716B.trace" "gobmk_60B.trace" "xalancbmk_748B.trace" "h264ref_178B.trace" "bzip2_281B.trace" "sjeng_1109B.trace" "astar_23B.trace" "calculix_2655B.trace" "namd_400B.trace" "gromacs_0B.trace" "gcc_13B.trace" "sphinx3_883B.trace" "GemsFDTD_712B.trace" "soplex_217B.trace" "bwaves_98B.trace" "bwaves_1609B.trace" "zeusmp_300B.trace" "sphinx3_2520B.trace" "libquantum_964B.trace" "wrf_1212B.trace" "perlbench_135B.trace" "zeusmp_100B.trace" "gcc_39B.trace" "libquantum_1210B.trace" "hmmer_546B.trace" "povray_711B.trace" "leslie3d_1186B.trace" "zeusmp_600B.trace" "astar_313B.trace" "tonto_2049B.trace" "soplex_66B.trace" "bzip2_183B.trace" "calculix_2670B.trace" "h264ref_351B.trace" "gamess_316B.trace" "astar_163B.trace" "lbm_94B.trace" "hmmer_397B.trace" "omnetpp_17B.trace" "lbm_564B.trace" "cactusADM_1495B.trace" "libquantum_1735B.trace" "milc_744B.trace" "povray_250B.trace" "sjeng_1966B.trace" "gromacs_1B.trace" "namd_851B.trace" "mcf_46B.trace" "perlbench_53B.trace" "bwaves_1861B.trace" "leslie3d_94B.trace" "milc_360B.trace" "soplex_205B.trace" "leslie3d_1116B.trace" "mcf_158B.trace" "hmmer_7B.trace" "mcf_250B.trace" "lbm_1004B.trace" "GemsFDTD_109B.trace") 
crc2_cloud_app=("streaming_phase4_core1.trace" "streaming_phase4_core0.trace" "streaming_phase0_core2.trace" "nutch_phase5_core3.trace" "nutch_phase0_core3.trace" "classification_phase2_core3.trace" "classification_phase0_core1.trace" "cassandra_phase1_core3.trace" "streaming_phase5_core3.trace" "streaming_phase1_core2.trace" "nutch_phase3_core3.trace" "nutch_phase3_core0.trace" "nutch_phase1_core3.trace" "cloud9_phase5_core2.trace" "cloud9_phase4_core1.trace" "classification_phase3_core1.trace" "cassandra_phase5_core2.trace" "cassandra_phase5_core0.trace" "cassandra_phase3_core1.trace" "cassandra_phase1_core2.trace" "cassandra_phase0_core2.trace" "streaming_phase4_core3.trace" "nutch_phase0_core1.trace" "cloud9_phase4_core0.trace" "classification_phase4_core3.trace" "classification_phase2_core1.trace" "nutch_phase4_core3.trace" "cloud9_phase2_core2.trace" "classification_phase5_core2.trace" "cassandra_phase3_core0.trace" "nutch_phase4_core1.trace" "cloud9_phase0_core1.trace" "classification_phase3_core0.trace" "cassandra_phase1_core1.trace" "streaming_phase0_core0.trace" "nutch_phase3_core1.trace" "cloud9_phase5_core3.trace" "classification_phase5_core0.trace" "classification_phase4_core0.trace" "streaming_phase5_core1.trace" "streaming_phase3_core0.trace" "cassandra_phase2_core2.trace" "streaming_phase1_core0.trace" "cloud9_phase3_core0.trace" "cloud9_phase4_core3.trace" "cassandra_phase2_core3.trace" "nutch_phase4_core0.trace" "cassandra_phase2_core0.trace" "nutch_phase5_core2.trace" "nutch_phase0_core0.trace" "cassandra_phase3_core2.trace" "streaming_phase3_core2.trace" "cloud9_phase3_core3.trace" "cloud9_phase3_core1.trace" "cloud9_phase2_core1.trace" "cloud9_phase0_core0.trace" "cassandra_phase4_core3.trace" "streaming_phase0_core1.trace" "nutch_phase1_core1.trace" "classification_phase3_core3.trace" "classification_phase1_core0.trace" "classification_phase0_core0.trace" "cassandra_phase5_core3.trace" "nutch_phase3_core2.trace" "classification_phase1_core3.trace" "streaming_phase3_core3.trace" "cloud9_phase2_core0.trace" "classification_phase5_core3.trace" "cassandra_phase4_core2.trace" "streaming_phase2_core0.trace" "classification_phase2_core0.trace" "classification_phase2_core2.trace" "cassandra_phase4_core1.trace" "streaming_phase3_core1.trace" "cloud9_phase4_core2.trace" "nutch_phase4_core2.trace" "nutch_phase1_core2.trace" "streaming_phase5_core2.trace" "cloud9_phase5_core0.trace" "cloud9_phase1_core2.trace" "cassandra_phase4_core0.trace" "cassandra_phase3_core3.trace" "cassandra_phase0_core3.trace" "cassandra_phase0_core1.trace" "streaming_phase1_core1.trace" "cloud9_phase3_core2.trace" "cloud9_phase1_core3.trace" "cassandra_phase5_core1.trace" "streaming_phase0_core3.trace" "classification_phase0_core2.trace" "cassandra_phase1_core0.trace" "streaming_phase2_core1.trace" "streaming_phase1_core3.trace" "classification_phase5_core1.trace" "classification_phase4_core1.trace" "streaming_phase5_core0.trace" "streaming_phase2_core2.trace" "cassandra_phase2_core1.trace" "cassandra_phase0_core0.trace" "nutch_phase0_core2.trace" "cloud9_phase5_core1.trace" "classification_phase3_core2.trace" "streaming_phase2_core3.trace" "classification_phase1_core2.trace" "streaming_phase4_core2.trace" "nutch_phase1_core0.trace" "classification_phase1_core1.trace" "classification_phase0_core3.trace" "cloud9_phase1_core0.trace" "cloud9_phase1_core1.trace" "nutch_phase2_core0.trace" "nutch_phase5_core0.trace" "classification_phase4_core2.trace" "nutch_phase5_core1.trace" "cloud9_phase2_core3.trace" "nutch_phase2_core1.trace" "nutch_phase2_core3.trace" "cloud9_phase0_core2.trace" "nutch_phase2_core2.trace" "cloud9_phase0_core3.trace")
dpc3_spec_app=("482.sphinx3-1522B.champsimtrace" "654.roms_s-1613B.champsimtrace" "605.mcf_s-472B.champsimtrace" "625.x264_s-20B.champsimtrace" "602.gcc_s-1850B.champsimtrace" "623.xalancbmk_s-325B.champsimtrace" "444.namd-166B.champsimtrace" "05.mcf_s-1152B.champsimtrace" "603.bwaves_s-1740B.champsimtrace" "648.exchange2_s-584B.champsimtrace" "435.gromacs-226B.champsimtrace" "447.dealII-3B.champsimtrace" "444.namd-321B.champsimtrace" "481.wrf-1170B.champsimtrace" "453.povray-252B.champsimtrace" "403.gcc-48B.champsimtrace" "603.bwaves_s-1080B.champsimtrace" "648.exchange2_s-72B.champsimtrace" "620.omnetpp_s-141B.champsimtrace" "458.sjeng-283B.champsimtrace" "450.soplex-247B.champsimtrace" "607.cactuBSSN_s-2421B.champsimtrace" "638.imagick_s-4128B.champsimtrace" "433.milc-274B.champsimtrace" "641.leela_s-602B.champsimtrace" "600.perlbench_s-210B.champsimtrace" "473.astar-153B.champsimtrace" "605.mcf_s-994B.champsimtrace" "410.bwaves-1963B.champsimtrace" "654.roms_s-842B.champsimtrace" "444.namd-33B.champsimtrace" "459.GemsFDTD-1320B.champsimtrace" "481.wrf-816B.champsimtrace" "437.leslie3d-273B.champsimtrace" "648.exchange2_s-1227B.champsimtrace" "458.sjeng-767B.champsimtrace" "625.x264_s-33B.champsimtrace" "607.cactuBSSN_s-3477B.champsimtrace" "623.xalancbmk_s-10B.champsimtrace" "625.x264_s-12B.champsimtrace" "605.mcf_s-1536B.champsimtrace" "456.hmmer-327B.champsimtrace" "648.exchange2_s-353B.champsimtrace" "435.gromacs-134B.champsimtrace" "weights-and-simpoints-speccpu.tar" "459.GemsFDTD-1491B.champsimtrace" "620.omnetpp_s-874B.champsimtrace" "453.povray-887B.champsimtrace" "621.wrf_s-8065B.champsimtrace" "445.gobmk-30B.champsimtrace" "473.astar-42B.champsimtrace" "416.gamess-875B.champsimtrace" "641.leela_s-1052B.champsimtrace" "401.bzip2-226B.champsimtrace" "603.bwaves_s-3699B.champsimtrace" "459.GemsFDTD-1418B.champsimtrace" "657.xz_s-3167B.champsimtrace" "454.calculix-460B.champsimtrace" "641.leela_s-334B.champsimtrace" "464.h264ref-30B.champsimtrace" "649.fotonik3d_s-7084B.champsimtrace" "627.cam4_s-490B.champsimtrace" "401.bzip2-7B.champsimtrace" "631.deepsjeng_s-928B.champsimtrace" "429.mcf-51B.champsimtrace" "644.nab_s-9322B.champsimtrace" "627.cam4_s-573B.champsimtrace" "605.mcf_s-1554B.champsimtrace" "401.bzip2-38B.champsimtrace" "444.namd-426B.champsimtrace" "621.wrf_s-575B.champsimtrace" "71.omnetpp-188B.champsimtrace" "401.bzip2-277B.champsimtrace" "410.bwaves-945B.champsimtrace" "648.exchange2_s-387B.champsimtrace" "462.libquantum-714B.champsimtrace" "444.namd-44B.champsimtrace" "623.xalancbmk_s-165B.champsimtrace" "465.tonto-44B.champsimtrace" "459.GemsFDTD-1211B.champsimtrace" "445.gobmk-17B.champsimtrace" "625.x264_s-39B.champsimtrace" "638.imagick_s-824B.champsimtrace" "459.GemsFDTD-765B.champsimtrace" "456.hmmer-88B.champsimtrace" "429.mcf-184B.champsimtrace" "603.bwaves_s-2609B.champsimtrace" "628.pop2_s-17B.champsimtrace" "649.fotonik3d_s-1B.champsimtrace" "465.tonto-1914B.champsimtrace" "458.sjeng-31B.champsimtrace" "400.perlbench-41B.champsimtrace" "445.gobmk-36B.champsimtrace" "625.x264_s-18B.champsimtrace" "605.mcf_s-665B.champsimtrace" "482.sphinx3-417B.champsimtrace" "623.xalancbmk_s-202B.champsimtrace" "654.roms_s-1021B.champsimtrace" "603.bwaves_s-5359B.champsimtrace" "437.leslie3d-149B.champsimtrace" "649.fotonik3d_s-8225B.champsimtrace" "473.astar-359B.champsimtrace" "482.sphinx3-1100B.champsimtrace" "602.gcc_s-2375B.champsimtrace" "605.mcf_s-484B.champsimtrace" "602.gcc_s-2226B.champsimtrace" "429.mcf-217B.champsimtrace" "403.gcc-16B.champsimtrace" "435.gromacs-111B.champsimtrace" "481.wrf-455B.champsimtrace" "649.fotonik3d_s-10881B.champsimtrace" "464.h264ref-64B.champsimtrace" "483.xalancbmk-716B.champsimtrace" "638.imagick_s-10316B.champsimtrace" "600.perlbench_s-1273B.champsimtrace" "648.exchange2_s-1712B.champsimtrace" "437.leslie3d-134B.champsimtrace" "619.lbm_s-4268B.champsimtrace" "429.mcf-192B.champsimtrace" "450.soplex-92B.champsimtrace" "481.wrf-1254B.champsimtrace" "641.leela_s-800B.champsimtrace" "429.mcf-22B.champsimtrace" "654.roms_s-1390B.champsimtrace" "654.roms_s-1070B.champsimtrace" "648.exchange2_s-1511B.champsimtrace" "482.sphinx3-234B.champsimtrace" "437.leslie3d-271B.champsimtrace" "641.leela_s-149B.champsimtrace" "644.nab_s-12521B.champsimtrace" "603.bwaves_s-2931B.champsimtrace" "648.exchange2_s-1699B.champsimtrace" "453.povray-800B.champsimtrace" "481.wrf-196B.champsimtrace" "444.namd-120B.champsimtrace" "657.xz_s-56B.champsimtrace" "644.nab_s-7928B.champsimtrace" "619.lbm_s-2677B.champsimtrace" "654.roms_s-294B.champsimtrace" "481.wrf-1281B.champsimtrace" "644.nab_s-5853B.champsimtrace" "433.milc-337B.champsimtrace" "621.wrf_s-6673B.champsimtrace" "459.GemsFDTD-1169B.champsimtrace" "433.milc-127B.champsimtrace" "654.roms_s-293B.champsimtrace" "470.lbm-1274B.champsimtrace" "603.bwaves_s-891B.champsimtrace" "483.xalancbmk-127B.champsimtrace" "434.zeusmp-10B.champsimtrace" "657.xz_s-2302B.champsimtrace" "05.mcf_s-1644B.champsimtrace" "621.wrf_s-8100B.champsimtrace" "482.sphinx3-1297B.champsimtrace" "648.exchange2_s-210B.champsimtrace" "456.hmmer-191B.champsimtrace" "454.calculix-104B.champsimtrace" "444.namd-23B.champsimtrace" "483.xalancbmk-736B.champsimtrace" "400.perlbench-50B.champsimtrace" "458.sjeng-1088B.champsimtrace" "403.gcc-17B.champsimtrace" "462.libquantum-1343B.champsimtrace" "657.xz_s-4994B.champsimtrace" "600.perlbench_s-570B.champsimtrace" "445.gobmk-2B.champsimtrace" "644.nab_s-12459B.champsimtrace" "464.h264ref-57B.champsimtrace" "437.leslie3d-265B.champsimtrace" "436.cactusADM-1804B.champsimtrace" "619.lbm_s-3766B.champsimtrace" "607.cactuBSSN_s-4004B.champsimtrace" "602.gcc_s-734B.champsimtrace" "605.mcf_s-782B.champsimtrace" "623.xalancbmk_s-700B.champsimtrace" "648.exchange2_s-1247B.champsimtrace" "607.cactuBSSN_s-4248B.champsimtrace" "453.povray-576B.champsimtrace" "410.bwaves-2097B.champsimtrace" "654.roms_s-523B.champsimtrace" "435.gromacs-228B.champsimtrace" "482.sphinx3-1395B.champsimtrace" "641.leela_s-1083B.champsimtrace" "654.roms_s-1007B.champsimtrace" "437.leslie3d-232B.champsimtrace" "619.lbm_s-2676B.champsimtrace" "641.leela_s-862B.champsimtrace" "649.fotonik3d_s-1176B.champsimtrace" "623.xalancbmk_s-592B.champsimtrace" "464.h264ref-97B.champsimtrace")

pg_dir=""

if [ -z ${plotgen_dir+x} ]; then pg_dir=""; else pg_dir=$plotgen_dir; fi

echo ${pg_dir}

# Tanvir Workloads
for application in ${tanvir_applications[@]}
do
    echo "${pg_dir}plotgen --debug -i ./tanvir/sizes_*champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf"
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.pdf
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html -o ./graphs/block_sizes_buffer_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./tanvir/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./tanvir/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.pdf
done

# IPC 1 Workload
for application in ${ipc1_spec_app[@]}
do
    echo "${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_*champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf"
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filenameframes .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html -o ./graphs/block_sizes_buffer_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filenameframes .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.pdf
done
for application in ${ipc1_server_app[@]}
do
    echo "${pg_dir}plotgen --debug -i ./ipc1_server/sizes_*champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf"
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html -o ./graphs/block_sizes_buffer_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_server/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_server/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.pdf
done
for application in ${ipc1_client_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./ipc1_client/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf"
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html -o ./graphs/block_sizes_buffer_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.pdf
    ${pg_dir}plotgen --debug -i ./ipc1_client/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./ipc1_client/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.pdf
done

# CRC2
for application in ${crc2_cloud_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./crc2_cloud/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf"
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html -o ./graphs/block_sizes_buffer_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.pdf
    ${pg_dir}plotgen --debug -i ./crc2_cloud/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_cloud/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.pdf
done
for application in ${crc2_spec_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./crc2_spec/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf"
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html -o ./graphs/block_sizes_buffer_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.pdf
    ${pg_dir}plotgen --debug -i ./crc2_spec/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.pdf
done

for application in ${dpc3_spec_app[@]}
do
    echo "~/Workspace/plotgen/plotgen --debug -i ./crc2_spec/sizes_champsim*k/**/cpu0_L1I_num_cl_with_block_size.tsv ./crc2_spec/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv ./ipc1_client/sizes_champsim_simple_vcl_buffer_64_lru_8w/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./([a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf"
    ${pg_dir}plotgen --debug -i ./dpc3/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./dpc3/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./dpc3/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size.tsv ./dpc3/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}.tsv -o ./graphs/block_sizes_buffer_bar_${application}.html -o ./graphs/block_sizes_buffer_bar_${application}.pdf
    ${pg_dir}plotgen --debug -i ./dpc3/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./dpc3/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_accum_avg_bar_${application}_accesses_scaled.pdf
    ${pg_dir}plotgen --debug -i ./dpc3/sizes_champsim*k/${application}/cpu0_L1I_num_cl_with_block_size_accesses_scaled.tsv ./dpc3/sizes_champsim_vcl_buffer_*/${application}/cpu0_L1I_buffer_num_cl_with_block_size_accesses_scaled.tsv --no-columns --frame-names --filename --column-names --filename --select-frames .*${application}.* --join index --column-names --renameregex '\./.*/(sizes_champsim[a-zA-Z\-_0-9]+)\.*' --palette bright --plot bar --file ./raw_data/block_sizes_buffer_bar_${application}_accesses_scaled.tsv -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.html -o ./graphs/block_sizes_buffer_bar_${application}_accesses_scaled.pdf
done

echo "DONE"
