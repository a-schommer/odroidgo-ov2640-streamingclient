/*
 *  MIT License
 *  
 *  Copyright (c) 2019 Arnold Schommer
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */
 //====================================================================================

//display any debug messages?
/* intended levels:
   0 - *no* messages at all
   1 - only high-level progress messages, not really debug
   2 - most messages
   3 - quite verbose
*/
#define DEBUG_LEVEL 1
// display debug messages on the internal screen, too? (otherwise: just serial)
#undef SCREEN_DEBUG

// enable/disable profiling
#define PROFILING
//#undef profiling
// each <how many> runs to report profiling data?
#define PROFILING_STEPPING  10

#define SOFTAP_MODE       //The comment will be connected to the specified ssid

#define WIFI_SSID   "TTGO-CAMERA-8D:65"
#define WIFI_PASSWD "NotSoSafe"
#define STREAM_HOST "2.2.2.1"
#define STREAM_PORT 81
#define STREAM_URI  "/stream"
#define HTTP_BEGIN_PARAMETERS   STREAM_HOST, STREAM_PORT, STREAM_URI

// directory on SD card for (debug) image dump:
// #define CAPTURE_DIR "/capture"
#undef CAPTURE_DIR

// some "text" constants needed to recognize/parse the stream (content):
// each single "element" (image) of the stream roughly starts:
// <stream-part-header [2 lines]><length>\r\n\r\n
#define STREAM_PART_HEADER_1    "Content-Type: image/jpeg"
#define STREAM_PART_HEADER_2    "Content-Length: "
// (where <length> is the length in bytes as a decimal string) followed by the 
// content (binary!) and "closed" by \r\n-- <stream-part-trailer> <mime-boundary> \r\n

// how many buffers to use?
#define NUM_BUFFERS 2
// size of a single buffer:
#define BUFFER_SIZE 32768

// heaps to be reserved
#define HEAP_READER 16384
#define HEAP_DISPLAY 4096
