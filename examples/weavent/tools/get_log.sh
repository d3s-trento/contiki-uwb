#!/bin/sh
JOBS_DIR=${JOBS_DIR:-$(dirname $0)/../jobs}

if tar -tf $JOBS_DIR/job_$1.tar.gz job_$1/job.log >/dev/null 2>&1; then
	tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/job.log -O
else
	tar -xOzf $JOBS_DIR/job_$1.tar.gz job_$1/test.log -O
fi
