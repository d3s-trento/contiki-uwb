#!/bin/sh

for I in $@; do 
	echo "$I: "; 
	get_log.sh $I | \grep -n 'DW1000' | sort -k 3 | awk 'BEGIN {LAST=""; L_ID=""} {if (L_ID == $3) {print LAST; print $0}; LAST=$0; L_ID=$3}' | sed -r 's/^(.*)$/  \1/'
	#get_log.sh $I | sed -n '/test timeout started/,$p' | sed '/End test/q' | grep 'Lost' | wc -l; 
done
