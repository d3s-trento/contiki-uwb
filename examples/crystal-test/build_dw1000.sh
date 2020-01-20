#!/usr/bin/env bash

# Use this script to load a binary for the evb1000 to a physical
# device.
#
# E.g: To flash a binary of one simulation generated, issue the following:
#
# bash ./build_dw1000.sh experiments/configuration_x/simulation_1/crystal_test.bin

BIN_FILE="$1"

if [ -z "$BIN_FILE" ]
then
    echo "No file given. Abort..."
    exit 1
fi
if ! [ -f "$BIN_FILE" ]
then
    echo "File $BIN_FILE doesn't exist! Abort..."
    exit 1
fi

extension="${BIN_FILE##*.}"
if ! [[ ${extension,,} =~ "bin" ]]
then
    echo "The file given has not a binary (.bin) extension. Abort..."
    exit 1
fi

echo "Loading $BIN_FILE..."

STFLASH="st-flash"
STFLASH_FLAGS="--reset"
STFLASH_CMD="write"
STFLASH_MEM_ADDR="0x08000000"

$STFLASH $STFLASH_FLAGS $STFLASH_SERIAL_ARG $STFLASH_CMD $BIN_FILE $STFLASH_MEM_ADDR

