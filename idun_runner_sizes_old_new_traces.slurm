#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-04:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=NONE
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"
module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim4k" "champsim8k" "champsim16k" "champsim32k" "champsim64k" "champsim128k" "champsim256k")
#binaries=("champsim_vcl_buffer_64d_data_no_history" "champsim_vcl_buffer_64d_data_partial_h" "champsim_vcl_buffer_64d_no_history" "champsim_vcl_buffer_64d_partial_h" "champsim_vcl_buffer_64_f_data_no_history" "champsim_vcl_buffer_64_f_data_partial_h" "champsim_vcl_buffer_64_f_no_history" "champsim_vcl_buffer_64_f_partial_h" "champsim_vcl_buffer_64_lru_8w_no_history" "champsim_vcl_buffer_64_lru_8w_partial_h" "champsim_vcl_buffer_64_lru_8w_data_no_history" "champsim_vcl_buffer_64_lru_8w_data_partial_h")

#binaries=("champsim32k" "champsim64k" "champsim_line_distillation" "champsim_vcl_buffer_fdip_64d" "champsim_vcl_buffer_fdip_extend_64d")
#binaries=("champsim32k" "champsim64k" "champsim_line_distillation" "champsim_vcl_buffer_fdip_16way_64d" "champsim_vcl_buffer_fdip_15way_64d" "champsim_vcl_buffer_fdip_16way_extend_64d" "champsim_vcl_buffer_fdip_15way_extend_64d")
binaries=("champsim_data_32k")

for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/spec --nosub --output_dir /cluster/work/romankb/results/fixed_traces_vs_old/ipc1_spec/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log & # We got spec in dpc3
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/server --nosub --output_dir /cluster/work/romankb/results/fixed_traces_vs_old/ipc1_server/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log &
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/client --nosub --output_dir /cluster/work/romankb/results/fixed_traces_vs_old/ipc1_client/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log &
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/ipc1/spec --nosub --output_dir /cluster/work/romankb/results/fixed_traces_vs_old/ipc1_spec/sizes_${binary}_old/ >> ~/pyrunner_ipc1_spec_${binary}.log & # We got spec in dpc3
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/ipc1/server --nosub --output_dir /cluster/work/romankb/results/fixed_traces_vs_old/ipc1_server/sizes_${binary}_old/ >> ~/pyrunner_ipc1_server_${binary}.log &
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/ipc1/client --nosub --output_dir /cluster/work/romankb/results/fixed_traces_vs_old/ipc1_client/sizes_${binary}_old/ >> ~/pyrunner_ipc1_client_${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
