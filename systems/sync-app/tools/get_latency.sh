#!/bin/sh

if (( $# < 1 )); then
	echo "Should have at least one job_id"
	exit
fi

echo "job_id,node_id,epoch,repetition_i,duration"
for I in $@; do
	get_log.sh $I |\
		grep -E '\[gt\]l [0-9]+ [0-9]+' |\
		awk '{print "'$I',"substr($4,0, length($4)-8)","int($7/10)","($7%10)","substr($8,0,length($8)-1) }'
done


