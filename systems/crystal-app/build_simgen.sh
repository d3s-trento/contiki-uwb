#!/bin/bash

ENV=crystal_test.bin.env
make clean

rm -f $ENV

pwd 2>&1 | tee -a $ENV
export 2>&1 | tee -a $ENV

echo "-- git status -------" >> $ENV
git log -1 --format="%H" >> $ENV
git --no-pager status --porcelain 2>&1 | tee -a $ENV
git --no-pager diff >> $ENV

echo "-- build log --------" >> $ENV
make -j3 2>&1 | tee -a $ENV

echo "-- sndtbl.c --------" >> $ENV
cat sndtbl.c >> $ENV
rm -f sndtbl.c symbols.h symbols.c
