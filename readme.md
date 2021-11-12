# Lilygo Slideshow
Inspired by [this article](https://debugger.medium.com/how-to-build-a-very-slow-movie-player-in-2020-c5745052e4e4).  
Uses the LILYGO® TTGO T5-4.7 Inch E-paper ESP32 dev board.  
You'll want to add the SD-crd module to store all them frames.  

## Using
See the instructions from [here](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) for instructions on setting up the dev environment.
Once set, use like any other Arduino sketch.

## Generating the images
The JPEG decoder is a bit picky with relation to wht'a in the file, these commands worked for me:

```bash
mkdir frames

ffmpeg -ss 05:20 -i [fileName] -vf "scale=960:720,crop=960:540,curves=all='0/0 0.023/0.078 0.375/0.6 0.726/0.83 1/0.922'" -r 1 -qscale:v 2 frames/%06d.jpg

mogrify -colorspace Gray -quality 80 frames/*.jpg
```

Note that use of curves filter for improved quality when displayed on the e-ink screen.

This script was usefull for generating images tha look better on the e-ink display:
http://www.fmwconcepts.com/imagemagick/dualtonemap/index.php