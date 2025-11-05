#!/bin/sh

if (( $# < 1 )); then
	echo "Should have at least one job_id"
	exit
fi

echo "job_id,node_id,epoch,hop,scan"
for I in $@; do
	get_log.sh $I | grep '\[n\]epoch ' | tr ".'" " " | awk '{print '"$I"'","$5","$10","$12","$14}'
done
