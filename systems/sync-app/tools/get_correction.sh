#!/bin/sh

if (( $# != 1 )); then
	echo "Should have exactly one job_id $#"
	exit
fi

get_log.sh $1 | grep 'c-tsm' | grep 'Offset ' | grep -v ' of ' | grep -v 'WARN:Offset' | tr ".]'" ' ' | awk '{print $5","$10","$12} BEGIN {print "node_id,epoch,correction_ns"}'
