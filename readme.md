# Lilygo Slideshow

Inspired by [this article](https://debugger.medium.com/how-to-build-a-very-slow-movie-player-in-2020-c5745052e4e4).  
Uses the LILYGO® TTGO T5-4.7 Inch E-paper ESP32 dev board.  
You'll want to add the SD-crd module to store all them frames.

## Features

- Long battery life  
  Using deep sleep for efficient power use (actual battery life depends on the battery used)
- Playlist support  
  Prepare a card with multiple movies by putting each one in a separate folder. They will be played by order one after the other
- Save position  
  The displayed frame position is saved to the SD card from time to time so playback can continue where it stopped after a reset or power loss
- Adaptive drawing  
  Use special logic for dark frame to improve their display

## Using

1. Setup your machine for development - see the instructions from [here](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47)
2. Adjust the configuration parameters at the top of the sketch file to your liking
3. Upload the sketch to the board
4. Prepare an SD card with the desired movies - see instructions below
5. Insert the SD card to the built-in reader
6. Connect a battery
7. Very slow playback of the movies will start
8. Put it somewhere nice and enjoy

## Generating the images

The firmware looks for frames named with a specific pattern, in folders under the root of the SD card file system.  
For example: `/movie_a/000001.jpg`, `/movie_a/000002.jpg`, `/movie_a/000003.jpg` etc.  
The files have to be named sequentially with no gaps and use 6 digits, zero-padded numbers.

The JPEG decoder expects a grayscale color space and _no progressive_ encoding.

These commands worked for me on a Linux system, using `ffmpeg` and `imagemagick`:

```bash
# In the directory that contains the video file
mkdir frames

# Use ffmpeg to:
# - Extract frames as files with proper names
# - Scale and crop to 960x540 pixels
# - Apply curves adjustment
> ffmpeg -ss 00:24 -to 02:01:07 -i video.file.name.mp4 -vf "scale=-2:540,crop=960:540,eq=saturation=0,lutrgb='r=clipval:g=clipval:b=clipval',curves=all='0/0 0.06/0.23 0.13/0.41 0.200/0.55 0.37/0.70 0.63/0.84 1/1'" -r 1 -qscale:v 2 frames/%06d.jpg

# Use mogrify from imagemagick
# to apply a grayscale colorspace for the frames
> mogrify -colorspace Gray -quality 90 frames/*.jpg
```

Note the use of curves filter for improved quality when displayed on the e-ink screen.

## BOM (2021)
- [LILYGO® TTGO T5-4.7](https://www.aliexpress.com/item/1005002006058892.html)
- [SD card module](https://www.aliexpress.com/item/1005002317501092.html)
- 3.7v Li-Po battery (the larger the better)
- A picture frame
- Some foam board for mounting the display in the picture frame  

Total cost: about $65 