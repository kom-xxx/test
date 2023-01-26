#!/bin/sh

./capture.py &
pid=$!
sleep 1
cat /proc/${pid}/maps | \
    awk '{if ($6 != "") print $6}' | sort | uniq > shlibs.txt

