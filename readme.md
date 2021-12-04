# Lilygo Slideshow
Inspired by [this article](https://debugger.medium.com/how-to-build-a-very-slow-movie-player-in-2020-c5745052e4e4).  
Uses the LILYGO® TTGO T5-4.7 Inch E-paper ESP32 dev board.  
You'll want to add the SD-crd module to store all them frames.  

## Using
See the instructions from [here](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) for setting up the dev environment.
Once set, use like any other Arduino sketch.

## Generating the images
The firmware looks for frames named in a specific pattern, in folders under the root of the SD card file system.  
For example: `/movie_a/000001.jpg`, `/movie_a/000002.jpg`, `/movie_a/000003.jpg` etc.  
The files have to be named sequentially with no gaps and use 6 digits, zero-padded numbers.

The JPEG decoder expects a grayscale color space and no progressive images.

These commands worked for me on a Linux system, using `ffmpeg` and `imagemagick`:  
```bash
# In the directory that contains the video file
mkdir frames

ffmpeg -ss 00:24 -to 02:01:07 -i video.file.name.mp4 -vf "scale=-2:540,crop=960:540,eq=saturation=0,lutrgb='r=clipval:g=clipval:b=clipval',curves=all='0/0 0.06/0.23 0.13/0.41 0.200/0.55 0.37/0.70 0.63/0.84 1/1'" -r 1 -qscale:v 2 frames/%06d.jpg
mogrify -colorspace Gray -quality 90 frames/*.jpg
```

Note the use of curves filter for improved quality when displayed on the e-ink screen.
