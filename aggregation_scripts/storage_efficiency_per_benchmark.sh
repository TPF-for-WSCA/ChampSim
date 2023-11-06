#!/bin/bash
#SBATCH --job-name="storage_efficiency_calculation"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c12
#SBATCH --mem-per-cpu=8G
#SBATCH --time=12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/home/romankb/storage-efficicency-%j.err
#SBATCH -o /cluster/home/romankb/storage-efficicency-%j.out
module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")
baseline_configs=("sizes_champsim32k_fdip_btb8192")
vcl_configs=("sizes_champsim_vcl_buffer_fdip_btb8192_64d")

chroot=""

if [ -z ${champsim_root+x} ]; then chroot=""; else chroot=$champsim_root; fi

echo ${chroot}

old_dir=pwd

for benchmark in ${benchmarks[@]}
do
    echo "Handling ${benchmark}"
    for config in ${baseline_configs[@]}
    do
        echo "\tHandling ${config}"
    	python ~/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm &
    done
    for config in ${vcl_configs[@]}
    do
        echo "\tHandling ${config}"
    	python ~/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm --vcl-configuration 4 8 12 16 16 16 20 28 36 44 48 64 64 64 &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done



echo "DONE"