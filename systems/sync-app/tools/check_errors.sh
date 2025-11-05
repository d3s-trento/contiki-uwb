#!/bin/sh

for I in $@; do 
	echo "$I: "; 
	get_log.sh $I | sed -n '/test timeout started/,$p' | sed '/End test/q' | grep -v 'samul.*Slots:' | grep 'ERR' | sed -r 's/^(.*)$/  \1/';
done
