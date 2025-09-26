#!/bin/sh

if [ -z "$JOBS_DIR" ]; then
	>&2 echo "JOBS_DIR is not set"
	exit 254
fi

if [ $# -lt 2 ]; then
	>&2 echo "You need at least 2 arguments"
    exit 254
fi

F1=$(tar -tf $JOBS_DIR/job_$1.tar.gz | xargs -n1 basename | grep '.bin$')
F2=$(tar -tf $JOBS_DIR/job_$2.tar.gz | xargs -n1 basename | grep '.bin$')

if [ "$F1" != "$F2" ]; then
	echo "List of bin files is different"
	exit 1
fi

RES=0

for FILE in $F1;
do
	(diff <(tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/$FILE -O) <(tar -xOzf $JOBS_DIR/job_$2.tar.gz job_$2/$FILE -O)) > /dev/null

	if [ $? -eq 0 ];
	then
		echo "$FILE is equal"
	else
		echo "$FILE is different"
		RES=1
	fi
done

exit $RES

# for 
# 
# 
# 
# 
# 
# 
# if [ $? -eq 0 ];
# then
# 	echo 'sink is equal'
# else
# 	echo 'sink is different'
# fi
# 
# (diff <(tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/peripheral.bin -O) <(tar -xOzf $JOBS_DIR/job_$2.tar.gz job_$2/peripheral.bin -O)) > /dev/null
# 
# if [ $? -eq 0 ];
# then
# 	echo 'peripheral is equal'
# else
# 	echo 'peripheral is different'
# fi
