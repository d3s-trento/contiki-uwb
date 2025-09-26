#!/bin/sh
JOBS_DIR=${JOBS_DIR:-$(dirname $0)/../jobs}

LAST_JOB=$(ls $JOBS_DIR/job*.tar.gz | sort | tail -n 1);
LAST_JOB_ID=$( basename "$LAST_JOB" | cut -d'_' -f2 | cut -d'.' -f1)
$(dirname $0)/filter_log.sh $LAST_JOB_ID
