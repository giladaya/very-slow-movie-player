#!/bin/bash

# Remove empty frames
# Expect input frames to be in folder __frames_raw under current dirctory
#
# Command:
# ./02_clean.sh

# fraction between 0 and 1
THRESH_DETAILS=0.02
THRESH_LOW=0.0002
THRESH_HIGH=0.9999

# Remove empty frames based on standard deviation
# See: https://legacy.imagemagick.org/discourse-server/viewtopic.php?t=24529
echo "Remove empty frames"
for image_file in __frames_raw/*.jpg
do
  std=`convert $image_file -format "%[fx:standard_deviation]" info:`
  stdt=`convert $image_file -format "%[fx:standard_deviation<$THRESH_DETAILS?1:0]" info:`
  # echo "Image $image_file STD $std $stdt"

  if [ $stdt -eq 1 ]; then
    echo "Removing essentially blank image $image_file STD $std / $stdt"
    mv $image_file __frames_rejects/
  # else
  #   echo "not black image"
  fi
done

# Remove empty frames based on brightness
# See: https://legacy.imagemagick.org/discourse-server/viewtopic.php?t=17111
for image_file in __frames_raw/*.jpg
do
  mean=`convert $image_file -format "%[mean]" info:`

  # Debug
  # meanNorm=`convert xc: -format "%[fx:($mean/quantumrange)]" info:`
  # echo "Image $image_file mean $meanNorm"

  meantest=`convert xc: -format "%[fx:($mean/quantumrange)<$THRESH_LOW?1:0]" info:`
  if [ $meantest -eq 1 ]; then
    echo "Removing essentially black image $image_file"
    mv $image_file __frames_rejects/
  # else
  #   echo "not black image"
  fi

  meantest=`convert xc: -format "%[fx:($mean/quantumrange)>$THRESH_HIGH?1:0]" info:`
  if [ $meantest -eq 1 ]; then
    echo "Removing essentially white image $image_file"
    mv $image_file __frames_rejects/
  # else
  #   echo "not white image"
  fi
done
