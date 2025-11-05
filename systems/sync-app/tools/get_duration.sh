#!/bin/sh

if (( $# != 1 )); then
	echo "Should have exactly one job_id $#"
	exit
fi

get_log.sh $1 | grep 'rtick passed ' |  tr ".]'" ' ' | awk '{print $5","$10","$15} BEGIN {print "node_id,epoch,duration_us"}'
