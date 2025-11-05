#!/bin/sh

echo "job_id,epoch,last_rx"
for I in $@; do 
	get_log.sh $I  | grep 'LAST RX' | cut -d"'" -f 2 | awk '{print '"$I"'","$2","$5}';
done
