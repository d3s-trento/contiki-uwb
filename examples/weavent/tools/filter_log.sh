#!/bin/sh
JOBS_DIR=${JOBS_DIR:-$(dirname $0)/../jobs}

$(dirname $0)/get_log.sh $1 | grep '+++'
