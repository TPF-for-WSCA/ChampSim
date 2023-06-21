#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"
module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
binaries=("champsim_data_4k" "champsim_data_8k" "champsim_data_16k" "champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_data_256k")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --instructions 200000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/crc2/cloud --nosub --intel --output_dir /cluster/work/romankb/results/size_sensitivity/crc2_cloud/sizes_${binary}/ >> ~/pyrunner_crc2_cloud_${binary}.log
    python /cluster/home/romankb/ChampSim/data_collector.py --instructions 200000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/crc2/spec --nosub --intel --output_dir /cluster/work/romankb/results/size_sensitivity/crc2_spec/sizes_${binary}/ >> ~/pyrunner_crc2_spec_${binary}.log
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

