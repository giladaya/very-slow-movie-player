#!/bin/bash

# Rename files to be in continuous sequence 
# Expect input frames to be in folder __frames_raw under current dirctory
#
# Command:
# ./03_resequence.sh

# Resequence files
echo "Resequencing frames"
mkdir -p frames_out
fileNum=1
for image_file in __frames_raw/*.jpg 
do
  new=$(printf "frames_out/%06d.jpg" "$fileNum")
  # echo "move $image_file to $new"
  # mv -i -- "$image_file" "$new"
  mv -- "$image_file" "$new"

  ((fileNum++))
done

# Cleanup
echo "Cleanup"
rm -rf __frames_raw
# rm -rf __frames_rejects

echo "Done"