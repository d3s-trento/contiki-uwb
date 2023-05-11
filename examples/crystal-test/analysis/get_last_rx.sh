#!/bin/sh
get_log.sh $1  | grep 'LAST RX' | cut -d"'" -f 2 | awk 'BEGIN{print "epoch,last_rx"} {print $2","$5}'
