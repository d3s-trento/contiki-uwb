JOBS_DIR=${JOBS_DIR:-$(dirname $0)/../jobs}

(diff <(tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/sink.bin -O) <(tar -xOzf $JOBS_DIR/job_$2.tar.gz job_$2/sink.bin -O)) > /dev/null

if [ $? -eq 0 ];
then
	echo 'sink is equal'
else
	echo 'sink is different'
fi

(diff <(tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/peripheral.bin -O) <(tar -xOzf $JOBS_DIR/job_$2.tar.gz job_$2/peripheral.bin -O)) > /dev/null

if [ $? -eq 0 ];
then
	echo 'peripheral is equal'
else
	echo 'peripheral is different'
fi
