#!/bin/sh

if [ $# -lt 1 ]; then
	>&2 echo "You need at least 1 arguments"
    exit 254
fi

get_log.sh $1 | \
	grep 'STATETIME' | \
	awk '{print gensub(/\.evb1000/, "", "g", $4)","$8""$10""$12""$14""$16""$18""gensub(/'"'"'/,"","g",$20)} BEGIN {print "node_id,epoch,idle,tx_preamble,tx_data,rx_hunting,rx_preamble,rx_data"}'
