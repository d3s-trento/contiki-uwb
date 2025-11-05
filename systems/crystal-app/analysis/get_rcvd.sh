#!/bin/sh
get_log.sh $1 | grep 'B [0-9]\+:[0-9]\+ [0-9]\+ [0-9]\+' | awk '{print gensub(/\.evb1000/, "", "g", $4)","gensub(/:/,",","g",$7)","$8","gensub(/'"'"'/, "", "g", $9)} BEGIN{print "node_id,epoch,src,seqn,app_seqn"}'
