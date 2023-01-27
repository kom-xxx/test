#!/bin/sh

mkdir -p output

./eos_example &
job=$!
sleep 3

kill -INT $job
sleep 1

grep moov output/camera.mp4
