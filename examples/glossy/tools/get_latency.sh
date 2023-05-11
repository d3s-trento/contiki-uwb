#!/bin/sh
get_log.sh $1 |\
	grep -E '\[gt\]l [0-9]+ [0-9]+' |\
	awk 'BEGIN {print "node_id,epoch,repetition_i,duration"} {print substr($4,0, length($4)-8)","int($7/50)","($7%50)","substr($8,0,length($8)-1) }'
