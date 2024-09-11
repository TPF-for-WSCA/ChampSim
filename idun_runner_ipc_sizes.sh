#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=NONE
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"
module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0

binary_dir=("micro-rebuttal-16-32" "micro-new-traces" "micro-acic" "micro" "micro-rebuttal-predictor" "size_sensitivity" "way_sensitivity" "micro-rebuttal")
#binaries=("champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_vcl_buffer_64d_data")
#binaries=("champsim_data_16k" "champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_vcl_buffer_64d_data" "champsim_vcl_buffer_64d_lru_data" "champsim_vcl_buffer_64_lru_2w_data" "champsim_vcl_buffer_64_lru_4w_data" "champsim_vcl_buffer_64_lru_8w_data" "champsim_vcl_buffer_64_3lip_d_data" "champsim_vcl_buffer_64_4lip_d_data" "champsim_vcl_buffer_64_lip_d_data" "champsim_vcl_buffer_64_3lip__lru_8w_data" "champsim_vcl_buffer_64_4lip__lru_8w_data" "champsim_vcl_buffer_64_lip__lru_8w_data")
#binaries=("champsim_data_16k" "champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_vcl_buffer_64_f_data" "champsim_vcl_buffer_64_lru_data" "champsim_vcl_buffer_64_lru_2w_data" "champsim_vcl_buffer_64_lru_4w_data" ""champsim_vcl_buffer_64_lru_8w_data"" "champsim_vcl_buffer_64d_data" "champsim_vcl_buffer_128_f_data" "champsim_vcl_buffer_128_lru_data" "champsim_vcl_buffer_128_lru_2w_data" "champsim_vcl_buffer_128_lru_4w_data" ""champsim_vcl_buffer_128_lru_8w_data"" "champsim_vcl_buffer_128d_data" "champsim_simple_vcl_buffer_64_lru_8w")
for dir in ${binary_dir[@]}
do
    for bin in /cluster/home/romankb/ChampSim/bin/$dir/*
    do
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=16G -n1 -c1 -t02:30:00 -o /cluster/work/romankb/latency-%j.out -e /cluster/work/romankb/latency-%j.err python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000         --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/spec   --nosub --output_dir /cluster/work/romankb/results/$(basename ${dir})/ipc1_spec/sizes_$(basename ${bin})/      >> /cluster/work/romankb/pyrunner_latency_fixed_spec_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=16G -n1 -c1 -t02:30:00 -o /cluster/work/romankb/latency-%j.out -e /cluster/work/romankb/latency-%j.err python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000         --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/server --nosub --output_dir /cluster/work/romankb/results/$(basename ${dir})/ipc1_server/sizes_$(basename ${bin})/    >> /cluster/work/romankb/pyrunner_latency_fixed_server_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=16G -n1 -c1 -t02:30:00 -o /cluster/work/romankb/latency-%j.out -e /cluster/work/romankb/latency-%j.err python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000         --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/client --nosub --output_dir /cluster/work/romankb/results/$(basename ${dir})/ipc1_client/sizes_$(basename ${bin})/    >> /cluster/work/romankb/pyrunner_latency_fixed_client_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=16G -n1 -c1 -t02:30:00 -o /cluster/work/romankb/google-%j.out  -e /cluster/work/romankb/google-%j.err  python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/google/whiskey             --nosub --output_dir /cluster/work/romankb/results/$(basename ${dir})/google_whiskey/sizes_$(basename ${bin})/ >> /cluster/work/romankb/pyrunner_google_whiskey_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=16G -n1 -c1 -t02:30:00 -o /cluster/work/romankb/google-%j.out  -e /cluster/work/romankb/google-%j.err  python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/google/merced              --nosub --output_dir /cluster/work/romankb/results/$(basename ${dir})/google_merced/sizes_$(basename ${bin})/  >> /cluster/work/romankb/pyrunner_google_merced_$(basename ${bin}).log  &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=16G -n1 -c1 -t02:30:00 -o /cluster/work/romankb/google-%j.out  -e /cluster/work/romankb/google-%j.err  python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/google/delta               --nosub --output_dir /cluster/work/romankb/results/$(basename ${dir})/google_delta/sizes_$(basename ${bin})/   >> /cluster/work/romankb/pyrunner_google_delta_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=16G -n1 -c1 -t02:30:00 -o /cluster/work/romankb/google-%j.out  -e /cluster/work/romankb/google-%j.err  python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/google/charlie             --nosub --output_dir /cluster/work/romankb/results/$(basename ${dir})/google_charlie/sizes_$(basename ${bin})/ >> /cluster/work/romankb/pyrunner_google_charlie_$(basename ${bin}).log &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
 
