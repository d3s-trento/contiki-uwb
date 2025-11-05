#!/bin/sh
 get_log.sh $1 | grep '\[crystal_tsm\]IS ORIG [0-9]\+' | tr -d "'" | awk 'BEGIN{print "epoch"} {print $8}'
