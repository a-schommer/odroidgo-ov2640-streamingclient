# ODROID-GO OV2640 streaming client

[tocstart]: # (toc start)

  * [Introduction](#introduction)
  * [Setting up](#setting-up)
  * [Environment](#environment)
  * [Developer Notes](#dev-notes)
  * [License](#license)

[tocend]: # (toc end)

## Introduction

You can meanwhile find lots of ESP-Boards with OV2640; most of them even come with a certain demo app that (also) creates a simple http stream of the pictures taken.

This sketch displays the video stream as provided by this demo app on the (internal) LCD of an ODROID-GO.

My sketch - to a certain degree - supports recording the recieved images on the SD card. This is not well tested and from previous experiments i remember that the file system has a poor performance if "many" files show up in the same directory - so please don't rely on this.

Please do not use this sketch in production - the camera app offers images/streams almost publicly.

## Setting Up

Take a look at config.h - this will not be a complete reference; if you need more, please take a look at the sources.

Usually you *must* adapt:
* WIFI_SSID & WIFI_PASSWD to whatever your camera is attached to (or spans itself)
* STREAM_HOST: the IP address (as a string) or hostname of the camera

Maybe interesting:
* DEBUG_LEVEL: the higher, the more messages you will see
* SCREEN_DEBUG: if defined, the debug messages will (at least partially) be displayed on the LCD, too.
  Might be handy if you do not want to use the serial debug, but many of it will anyway remain (almost) invisible - or mess up the video stream.
* CAPTURE_DIR: if defined, it should point to a directory of you SD card. There the recieved images will be saved!
  Don't forget the leading slash; a trailing one is not needed. If it does not yet exist, the sketch will try to create the directory.
* STREAM_PORT (numeric) and STREAM_URI: if you modify the streaming app or use a different camera, you likely have have to adjust these, too
* PROFILING: if defined, some simple profiling is done - you will e.g. see how many fps you get (on average).

## Environment

What do you need to "use" this sketch?

* The Arduino IDE
  Well, it may work with the ESP IDF, but could not provide any help as i am not using that.

* A separate ESP32-Boards running the demo app (or at least something sufficiently "compatible")
  Hardware: e.g. https://www.banggood.com/x-p-1418433.html, https://www.amazon.com/dp/B07MC472S9
  If you lost the original app: The demo ESP32 / Camera / CameraWebServer coming with the ESP32 board manager.

* The ESP32 Board Manager (https://github.com/espressif/arduino-esp32)
  There are others and i would expect them to work, too - but i simply did not check that. 
  Bottom line: If you get strange compilation problems, give that one a try.
  This also contains the example camera app i run on the camera board (https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Camera/CameraWebServer)
  
* An ODROID-GO to run *this* sketch on (https://www.hardkernel.com/shop/odroid-go)
  If you do not know how to "upload" a sketch ... well, i will not explain this; if you are completely lost: start at the official wiki, https://wiki.odroid.com/odroid_go/odroid_go. It does not provide a "quick start", but you should find all information you need there.

* The library framework provided by Hardkernel, a library called "ODROID-GO" (https://github.com/hardkernel/ODROID-GO)
  Mainly "just" the LCD interface (including JPEG decoding!) is used, but i forgot if i took the (optional) SD access from there or from standard libs.
  Note: This is not available in the Arduino IDE's Library Manager; you have to download it yourself and unpack it to the librarys path.

## Troubleshooting
Apart from intermediate mistakes i had little problems, so i also have quite few ideas concerning what might go wrong and how.

* If you do not get your setup working, check if you can recieve the stream on a "real" computer just accessing it via browser. If that does not work, the sketch very likely will fail, too (but unfortunately not vice versa). 
* The original app does not tell you the complete network setup on the integrated OLED (if there is one), but via serial debugging
* The camera module i own was specified to draw 1A of power supply. That means it does not have to work on a regular USB port.
  Well, this was half true in my tests: When attached to an USB port, the built-in OLED was that messy that i assumed a hardware issue - but the camera and serial debugging (as far as i recognized) were absolutely ok. Bottom line: if your on board display (of the camera module!) fails, either ignore it or try another power supply.

## Developer Notes

My primary goal was to show the camera stream (which before worked with a browser client on Windows) on an ESP32 based device - so i did not bother with questions like "how does video streaming via http work" but (more or less) reverse engineered what i found. There were even some oddities concerning the "protocol" i gently ignored as my sketch worked.

There may be quite different sources for the video stream that work, too - but i do not know much about that. If you want to give it a try: the demo app by default provides images of 320*240 pixels - the resolution of the ODROID-GO LCD. This is not checked by the sketch nor absolutely necessary, but if the provided images are larger, just the top left corner will be shown. (If the images are smaller, the remainder of the screen will stay black)

You can check the video stream "in parallel" from your PC via browser - and even use the GUI on port 80 to change settings.

I used a fixed buffer size; in my experiments 64 KB were sufficient.

Currently, the sketch does not handle losing the (IP-) connection to the camera - simply because i did not yet experience it.

## License

The MIT License (MIT)

Copyright (c) 2019 by Arnold Schommer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
