#!/bin/bash
#SBATCH --job-name="btb_offset_calculation"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c12
#SBATCH --mem-per-cpu=8G
#SBATCH --time=12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/storage-efficicency-%j.err
#SBATCH -o /cluster/work/romankb/storage-efficicency-%j.out
module load matplotlib/3.8.2-gfbf-2023b
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")
#benchmarks=("whiskey" "merced" "delta" "charlie")
baseline_config=("sizes_1k_btb" "sizes_2k_btb" "sizes_4k_btb" "sizes_8k_btb" "sizes_16k_btb" "sizes_32k_btb")

old_dir=pwd

for benchmark in ${benchmarks[@]}
do
    for run in $benchmark/${baseline_config[@]}
    do
        echo "Handling ${run}"
        python /cluster/home/romankb/ChampSim/aggregation_scripts/btb_offset_analysis.py $run &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

echo "DONE"
