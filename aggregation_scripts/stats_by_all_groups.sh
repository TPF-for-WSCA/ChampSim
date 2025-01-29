#!/bin/bash

old_dir=$(pwd)

for rdir in ./*;
do
	if [[ $rdir == *_18 ]];
	then
		cd $rdir
		echo "Running graphing script in dir $(pwd)"
		srun --account share-ie-idi -n1 -c20 --time=02:30:00 -o ~/graphing_by_application.out -e ~/graphing_by_application.err --mem-per-cpu=4G ~/ChampSim/aggregation_scripts/stats_by_group.sh &
		cd $old_dir
	fi
done

for job in `jobs -p`
do
	echo $job
	wait $job
done
