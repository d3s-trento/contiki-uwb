#!/bin/sh

get_log.sh $1 | grep "'\[n\]epoch " | awk 'BEGIN{print "node_id,epoch,hop"} {print substr($4,0,length($4)-8)", "$7", "substr($9,0,length($9)-1)}'
