#!/bin/sh

if [ $# -lt 1 ]; then
	>&2 echo "You need at least 1 arguments"
    exit 254
fi

get_log.sh $1 | \
	grep '\[tsm [0-9]*\]Slots:' | \
	awk '{print gensub(/\.evb1000/, "", "g", $4)","gensub(/\]Slots:/,"","g",$7)","gensub(/'"'"'/, "", "g", $8)} BEGIN {print "node_id,epoch,slots"}'
