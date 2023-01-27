#!/bin/sh

./eos_example &
job=$!
sleep 3
kill -INT $job
