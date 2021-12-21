#!/bin/bash

# Extract frames from a video
# Apply curves and color space conversion
#
# Command:
# ./01_extract.sh [file name] [start time] [end time]
#
# Example:
# ./01_extract.sh filepath.mkv 00:24 02:01:07

videoFileName=$1
startTime=$2
endTime=$3

# Extract frames
# Resize and apply curves
mkdir -p __frames_raw
mkdir -p __frames_rejects
ffmpeg -ss $startTime -to $endTime -i $videoFileName -vf "scale=-2:540,crop=960:540,eq=saturation=0,lutrgb='r=clipval:g=clipval:b=clipval',curves=all='0/0 0.06/0.23 0.13/0.41 0.200/0.55 0.37/0.70 0.63/0.84 1/1'" -r 1 -qscale:v 2 __frames_raw/%06d.jpg

# Convert color space
echo "Mogrify..."
mogrify -colorspace Gray -quality 90 __frames_raw/*.jpg
