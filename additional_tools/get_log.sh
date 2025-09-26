if [ -z "$JOBS_DIR" ]; then
	>&2 echo "JOBS_DIR is not set"
	exit 254
fi

if tar -tf $JOBS_DIR/job_$1.tar.gz job_$1/job.log >/dev/null 2>&1; then
	tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/job.log -O
else
	tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/test.log -O
fi
