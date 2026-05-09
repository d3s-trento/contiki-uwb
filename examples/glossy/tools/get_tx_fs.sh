#!/bin/sh

get_log.sh $1 |\
	grep "'\[n\]TX_FS E [0-9]\+" |\
	awk 'BEGIN{print "node_id,epoch,repetition_i"} {print gensub(/\.[a-zA-Z0-9]+/,"",1,$4)","int(gensub(/'"'"'/,"","g",$8)/50)","(gensub(/'"'"'/,"","g",$8)%50)}'
