#!/bin/sh

get_log.sh $1 |\
	grep "'\[n\]TX_FS E [0-9]\+" |\
	awk 'BEGIN{print "node_id,epoch,repetition_i"} {VAL=int(gensub(/'"'"'/,"","g",$8)); print gensub(/\.evb1000/,"","g",$4)","and(VAL, 0x1ffff)","and(rshift(VAL,17), 0xff)}'
# if [ $1 -lt 230 ] || [ $1 -gt 4000 ]; then
# 	get_log.sh $1 |\
# 		grep "'\[n\]TX_FS E [0-9]\+" |\
# 		awk 'BEGIN{print "node_id,epoch,repetition_i"} {print gensub(/\.evb1000/,"","g",$4)","int(gensub(/'"'"'/,"","g",$8)/100)","(gensub(/'"'"'/,"","g",$8)%100)}'
# else
# 	get_log.sh $1 |\
# 		grep "'\[n\]TX_FS E [0-9]\+" |\
# 		awk 'BEGIN{print "node_id,epoch,repetition_i"} {VAL=int(gensub(/'"'"'/,"","g",$8)); print gensub(/\.evb1000/,"","g",$4)","and(VAL, 0x1ffff)","and(rshift(VAL,17), 0xff)}'
# fi
