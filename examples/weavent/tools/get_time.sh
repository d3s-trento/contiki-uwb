#!/bin/sh
if [ $1 -lt 9415 ]; then
	get_log.sh $1 | grep 'eSFD' | awk 'BEGIN{print "node_id,epoch,repetition_i,expected,actual"} {print substr($4, 0, length($4)-8)", "int($7/100)", "($7 % 100)", "$9", "gensub(/'"'"'/,"", "g", $11)}'
else
	get_log.sh $1 |\
		grep -E '\[td\]l[0-9a-f]{8}[0-9a-f]{8}[0-9a-f]{8}(X)?' |\
		awk 'BEGIN {print "node_id,epoch,repetition_i,expected,actual,detect_only"} {W=substr($6, 8, length($6)-8); EPOCH=strtonum("0x" substr(W,0,8)); printf "%s,%d,%d,%d,%d,%d\n", substr($4,0, length($4)-8), EPOCH/100, EPOCH%100, strtonum("0x" substr(W,9,8)), strtonum("0x" substr(W,17,8)), length(W) >= 25 }'
fi
