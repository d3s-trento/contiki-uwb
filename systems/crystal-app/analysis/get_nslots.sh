#!/bin/sh
echo "job_id,node_id,epoch,nslot"

for I in $@; do
	get_log.sh $I | grep 'E [0-9]* NSLOTS [0-9]*' | grep -v 'MTA' | awk '{print '$I'","gensub(/\.evb1000/, "", "g", $4)","$7","gensub(/'"'"'/, "", "g", $9)} '
done

