#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=02-00:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"
module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim4k" "champsim8k" "champsim16k" "champsim32k" "champsim64k" "champsim128k" "champsim256k")
#binaries=("champsim_vcl_buffer_64d_data_no_history" "champsim_vcl_buffer_64d_data_partial_h" "champsim_vcl_buffer_64d_no_history" "champsim_vcl_buffer_64d_partial_h" "champsim_vcl_buffer_64_f_data_no_history" "champsim_vcl_buffer_64_f_data_partial_h" "champsim_vcl_buffer_64_f_no_history" "champsim_vcl_buffer_64_f_partial_h" "champsim_vcl_buffer_64_lru_8w_no_history" "champsim_vcl_buffer_64_lru_8w_partial_h" "champsim_vcl_buffer_64_lru_8w_data_no_history" "champsim_vcl_buffer_64_lru_8w_data_partial_h")

#binaries=("champsim16k" "champsim32k" "champsim64k" "champsim128k" "champsim_vcl_buffer_64d" "champsim_vcl_buffer_64_lru_2w" "champsim_vcl_buffer_64_lru_4w" "champsim_vcl_buffer_64_lru_8w" "champsim_vcl_buffer_64_3lip_d" "champsim_vcl_buffer_64_4lip_d" "champsim_vcl_buffer_64_lip_d" "champsim_vcl_buffer_64_2lip_d" "champsim_vcl_buffer_64_3lip__lru_8w" "champsim_vcl_buffer_64_4lip__lru_8w" "champsim_vcl_buffer_64_lip__lru_8w" "champsim_vcl_buffer_64_2lip__lru_8w")
binaries=("champsim16k" "champsim32k" "champsim64k" "champsim128k" "champsim_vcl_buffer_64d" "champsim_vcl_buffer_64_lru_2w" "champsim_vcl_buffer_64_lru_4w" "champsim_vcl_buffer_64_lru_8w" "champsim_vcl_buffer_64_3lip_d" "champsim_vcl_buffer_64_4lip_d" "champsim_vcl_buffer_64_lip_d" "champsim_vcl_buffer_64_2lip_d" "champsim_vcl_buffer_64_3lip__lru_8w" "champsim_vcl_buffer_64_4lip__lru_8w" "champsim_vcl_buffer_64_lip__lru_8w" "champsim_vcl_buffer_64_2lip__lru_8w")
#binaries=("champsim32k" "champsim64k" "champsim_simple_vcl_buffer_64_lru_8w")

for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --instructions 90000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --trace_format p --traces_directory /cluster/work/romankb/dataset/tanvir --output_dir /cluster/work/romankb/results/full_analysis_v8/tanvir/sizes_${binary}/ >> ~/pyrunner_${binary}.log & 
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

