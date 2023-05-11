#!/bin/sh

if [ $# -lt 1 ]; then
	>&2 echo "You need at least 1 arguments"
    exit 254
fi

get_log.sh $1 | grep -m 1 ' DEBUG:testbed-run: Candidate Nodes :' | cut -d' ' -f 7
