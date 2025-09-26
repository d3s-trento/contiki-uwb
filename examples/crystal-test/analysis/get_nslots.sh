#!/bin/sh
get_log.sh $1 | grep 'E [0-9]* NSLOTS [0-9]*' | awk '{print gensub(/\.evb1000/, "", "g", $4)","$7","gensub(/'"'"'/, "", "g", $9)} BEGIN{print "node_id,epoch,nslot"}'

