#!/bin/sh

get_log.sh $1 |\
	grep -E "'\[td\]k[0-9a-f]{12}'" |\
	awk 'BEGIN {print "node_id,repetition_i,epoch,detect_only,diff"} {VAL=int(strtonum("0x" substr($6, 8, length($6)-8))); print substr($4,0, length($4)-8)"," rshift(and(VAL,0xff0000000000),40)","rshift(and(VAL,0x00ffff800000),23)","rshift(and(VAL,0x000000400000),22)","and(VAL,0x0000003fffff)}'


# if [ $1 -lt 9415 ] && [ $1 -gt 4000 ]; then
# 	get_log.sh $1 | grep 'eSFD' | awk 'BEGIN{print "node_id,epoch,repetition_i,expected,actual"} {print substr($4, 0, length($4)-8)", "int($7/100)", "($7 % 100)", "$9", "gensub(/'"'"'/,"", "g", $11)}'
# else
# 	if [ $1 -lt 230 ]; then
# 		get_log.sh $1 |\
# 			grep -E "'\[td\]k[0-9a-f]{8}[0-9a-f]{8}(X)?'" |\
# 			awk 'BEGIN {print "node_id,epoch,repetition_i,diff,detect_only"} {W=substr($6, 8, length($6)-8); EPOCH=strtonum("0x" substr(W,0,8)); printf "%s,%d,%d,%d,%d\n", substr($4,0, length($4)-8), EPOCH/125, EPOCH%125, strtonum("0x" substr(W,9,8)), length(W) >= 17 }'
# 	else
# 		get_log.sh $1 |\
# 			grep -E "'\[td\]k[0-9a-f]{12}'" |\
# 			awk 'BEGIN {print "node_id,repetition_i,epoch,detect_only,diff"} {VAL=int(strtonum("0x" substr($6, 8, length($6)-8))); print substr($4,0, length($4)-8)"," rshift(and(VAL,0xff0000000000),40)","rshift(and(VAL,0x00ffff800000),23)","rshift(and(VAL,0x000000400000),22)","and(VAL,0x0000003fffff)}'
# 	fi
# fi
