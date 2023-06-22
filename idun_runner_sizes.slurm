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
module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
#binaries=("champsim4k" "champsim8k" "champsim16k" "champsim32k" "champsim64k" "champsim128k" "champsim256k")
binaries=("champsim16k" "champsim32k" "champsim64k" "champsim128k" "champsim_vcl_buffer_64_f" "champsim_vcl_buffer_64_lru" "champsim_vcl_buffer_64_lru_2w" "champsim_vcl_buffer_64_lru_4w" ""champsim_vcl_buffer_64_lru_8w"" "champsim_vcl_buffer_64d" "champsim_vcl_buffer_128_f" "champsim_vcl_buffer_128_lru" "champsim_vcl_buffer_128_lru_2w" "champsim_vcl_buffer_128_lru_4w" ""champsim_vcl_buffer_128_lru_8w"" "champsim_vcl_buffer_128d")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --instructions 90000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --trace_format p --traces_directory /cluster/work/romankb/dataset/tanvir --output_dir /cluster/work/romankb/results/full_analysis_v2/tanvir/sizes_${binary}/ >> ~/pyrunner_${binary}.log & 
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

