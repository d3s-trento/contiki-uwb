#!/bin/sh

if (( $# != 1 )); then
	echo "Should have exactly one job_id $#"
	exit
fi

get_log.sh $1 | grep 'c-tsm' | grep 'tref diff' | tr ".]'" ' ' | awk '{print $5","$10","$13} BEGIN {print "node_id,epoch,delay_tick"}'
