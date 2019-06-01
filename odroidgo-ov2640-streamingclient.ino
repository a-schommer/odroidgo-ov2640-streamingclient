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

#include "config.h"

//====================================================================================
//                                  Libraries

#include <odroid_go.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#ifdef CAPTURE_DIR
#include "FS.h"
#include "SD.h"
#endif

WiFiMulti wifiMulti;

//====================================================================================
// enable profiling or "comment it out"

#ifdef PROFILING
#include "profiling.h"

Profile profile_stream_open("http_stream_open()");
Profile profile_reader_wait("reader waits for free buffer");
Profile profile_read_network("network read");
Profile profile_display_wait("display() waits for image");
#ifdef CAPTURE_DIR
Profile profile_write_SD("writing file to SD card");
#endif
Profile profile_paint("decoding & displaying JPEG");

#define PROFILED_DO(profile, code)  PROFILED_RUN(profile, code)
#define PROFILED_START(profile)     profile.start()
#define PROFILED_END(profile)       profile.done()
#else
#define PROFILED_DO(profile, code)  code
#define PROFILED_START(profile)
#define PROFILED_END(profile)
#endif

//====================================================================================
// write a string to serial and - only if SCREEN_DEBUG - LCD:

#ifdef SCREEN_DEBUG
void debug_println(const char *msg) {
    if(strlen(msg) < 100) {
        GO.lcd.println(msg);
    } else {
        static char b[100];
        strncpy(b, msg, sizeof(b)-1);
        GO.lcd.printf("%s...\n", b);
    }
    Serial.println(msg);
}
#else
    #define debug_println Serial.println
#endif

#define debug_printf1ln(b, t, p1)       { snprintf(b, sizeof(b)-1, t, p1);     debug_println(b); }
#define debug_printf2ln(b, t, p1, p2)   { snprintf(b, sizeof(b)-1, t, p1, p2); debug_println(b); }
    
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0
#endif
#if DEBUG_LEVEL > 0
    #define debug1_println      debug_println
    #define debug1_printf1ln    debug_printf1ln
    #define debug1_printf2ln    debug_printf2ln
#else
    #define debug1_println(...)
    #define debug1_printf1ln(...)
    #define debug1_printf2ln(...)
#endif
#if DEBUG_LEVEL > 1
    #define debug2_println      debug_println
    #define debug2_printf1ln    debug_printf1ln
    #define debug2_printf2ln    debug_printf2ln
#else
    #define debug2_println(...)
    #define debug2_printf1ln(...)
    #define debug2_printf2ln(...)
#endif
#if DEBUG_LEVEL > 2
    #define debug3_println      debug_println
    #define debug3_printf1ln    debug_printf1ln
    #define debug3_printf2ln    debug_printf2ln
#else
    #define debug3_println(...)
    #define debug3_printf1ln(...)
    #define debug3_printf2ln(...)
#endif

//====================================================================================
//                                    Buffers
//====================================================================================
//uint8_t jpegBuffers[NUM_BUFFERS][BUFFER_SIZE];
//size_t jpegSizes[NUM_BUFFERS];      // gets the num of bytes being read as JPEG data
//buffer_state buffer_states[NUM_BUFFERS];

// possible buffer states
typedef enum { BUFFER_FREE, BUFFER_READING, BUFFER_FILLED, BUFFER_DISPLAYING } buffer_state;

//====================================================================================
// a class to define (one) buffer for a jpeg file - being read from net and being dsplayed after

class IOBuffer { 
    public:
        buffer_state state = BUFFER_FREE;
        static const size_t size = BUFFER_SIZE; // how much can the buffer hold?
        size_t usedSize;                        // ... and how much *does* it just contain?
        uint8_t data[BUFFER_SIZE];

        void waitForState(buffer_state wanted, short msDelay = 1) {
            while(this->state != wanted)
                delay(msDelay);
        }

        // change the state in a specified manner, returning "false" if the current state does not match
        bool acquire(buffer_state stateBefore, buffer_state stateAfter) {
            if(this->state == stateBefore) {
                this->state = stateAfter;
                return true;
            } else
                return false;
        }

        // wait (unlimited) for one state and then change it to another
        bool allocate(buffer_state stateBefore, buffer_state stateAfter) {
            this->waitForState(stateBefore);
            this->state = stateAfter;
        }
};

IOBuffer jpegBuffers[NUM_BUFFERS];

//====================================================================================
#ifdef CAPTURE_DIR
// dump some bytes to a file
void dumpToFile(fs::FS &fs, const char *path, const uint8_t *data, const size_t size) {
    static char msg[100];

    File file = fs.open(path, FILE_WRITE);
    if(!file) {
        debug1_printf1ln(msg, "Failed to open file '%s' for writing", path);
        return;
    }
    if(file.write(data, size) != size) {
        debug1_printf2ln(msg, "Failed to write %d bytes to file '%s'", size, path);
    }
    file.close();
}
#endif

//====================================================================================
// "framework" to read separate images from an HTTP stream:

HTTPClient http;
bool http_is_open = false;
static char debug_msg[100];
static char mime_boundary[128];

// read some line from the HTTP stream until (including, but not being saved) "\r\n"
// the result is a char * to the read content which is guaranteed to be 0-terminated, but if the line does not end before, this cannot be recognized!!!
#define GETLINE_CR  '\r'
#define GETLINE_LF  '\n'
#define GETLINE_STRING_END '\0'
#define GETLINE_ERROR_RETURN    ((char *)NULL)
char *http_get_line(void) {
    static int prev_char = GETLINE_STRING_END;
    static char lineBuffer[256];
    char *to = lineBuffer;
    size_t remaining_buffer = sizeof(lineBuffer)-1; // keep 1 byte safety margin for 0 terminator
    WiFiClient *stream = http.getStreamPtr();

    while(remaining_buffer > 0) {
        // wait for at least one character is available to be read
        while(stream->available() < 1) {
            if(stream->available() < 0) { // error
                return GETLINE_ERROR_RETURN;
            }
            delay(1);
        }
        
        // read one character, handle error
        int new_char = stream->read();
        if(new_char < 0)    return GETLINE_ERROR_RETURN;
        
        // save the new char to the result buffer
        *to++ = new_char;
        --remaining_buffer;
        
        // abort on 0 char being read
        if(new_char == GETLINE_STRING_END)    return lineBuffer;
        // if this char is \n and the previous was \r, abort reading & strip those two from the string result
        if((new_char == GETLINE_LF) && (prev_char == GETLINE_CR)) {
            to[-2] = GETLINE_STRING_END;
            break;
        }
        
        prev_char = new_char;
    }
    // terminate the result:
    *to = GETLINE_STRING_END;
    return lineBuffer;
}

// read some number of bytes from the HTTP stream
#define GETBIN_ERROR_RETURN     (-1)
size_t http_get_binary(uint8_t *to, size_t length) {
    WiFiClient *stream = http.getStreamPtr();
    size_t sum_BytesRead = 0;

    while(length > 0) {
        // wait for at least one character is available to be read
        while(stream->available() < 1) {
            if(stream->available() < 0) { // error
                return GETBIN_ERROR_RETURN;
            }
            delay(1);
        }
        
        // read at most what is requested / left of that:
        size_t bytesRead = stream->readBytes(to, length);
        sum_BytesRead += bytesRead;
        if(bytesRead <= 0)
            return sum_BytesRead;
        else if(bytesRead > length)
            debug1_printf2ln(debug_msg, "\t*** buffer overrun in http_get_binary: %d bytes reported, %d requested", bytesRead, length);
            // in the end, the binary chunk *was* read ... in a way ... so: don't treat this as an error ... here ...
        
        to     += bytesRead;
        length -= bytesRead;
    }

    return sum_BytesRead;
}

// "prepare" a "persistent" stream access:
bool http_stream_open(void) {
    static bool http_get_done = false;
    static int httpCode;

    if(! http_is_open) {
        http_get_done = false;
        snprintf(debug_msg, sizeof(debug_msg)-1, "[HTTP] begin(%s, %d, %s)", STREAM_HOST, STREAM_PORT, STREAM_URI);
        debug2_println(debug_msg);
        if(! http.begin(STREAM_HOST, STREAM_PORT, STREAM_URI)) {
            debug2_println("\tfailed!");
            return false;
        }
        http_is_open = true;
    }

    if(!http_get_done) {
        debug2_println("[HTTP] GET...");
        httpCode = http.GET();
        // httpCode will be negative on error
        if(httpCode <= 0) {
            debug2_printf1ln(debug_msg, "\tfailed, error: %s", http.errorToString(httpCode).c_str());
            return false;
        }
        // HTTP header has been send and Server response header has been handled
        debug2_printf1ln(debug_msg, "\t=> %d", httpCode);
        
        // "file" found at server?
        if(httpCode != HTTP_CODE_OK)    return false;

        http_get_done = true;
    }

    // (now, the stream seems open & the mime setup is known)
    return true;
}

// return either a quoted string or "empty line":
char *special_quoting(const char *org) {
    static char buffer[256];
    size_t l;
    
    if((org == NULL) || (*org == 0))
        return "empty line";

    l = strlen(org);
    buffer[0] = '\'';
    if(l < 256 - 3) {
        strcpy(buffer+1, org);
        buffer[1+l] = '\'';
        buffer[2+l] = '\0';
    } else {
        strncpy(buffer+1, org, 256-6);
        strcpy(buffer+251, "...'");
    }
    return buffer;
}

// (try to) get one image:
#define HTTP_GET_ERROR_RETURN   (-1)
size_t http_stream_get_jpeg(uint8_t *buffer, const size_t bufferSize)
{
    size_t announced_size, bytes_read, result = 0;
    char *line;
    bool found;
    static char mark1[] = STREAM_PART_HEADER_1,
                mark2[] = STREAM_PART_HEADER_2;

    debug2_println("http_stream_get_jpeg(): starting");

    PROFILED_START(profile_stream_open);
    bool stream_opened = http_stream_open();
    PROFILED_END(profile_stream_open);
    if(! stream_opened) {
        debug1_println("http_stream_get_jpeg(): http_stream_open() failed");
        return HTTP_GET_ERROR_RETURN;
    }

    // wait/check for new mime part "header":
    do  {
        if(!(line = http_get_line())) {
            debug1_println("http_stream_get_jpeg(): error reading MIME header (line 1)");
            return HTTP_GET_ERROR_RETURN;
        }
        found = strcmp(line, mark1) == 0;
        if(! found)
            debug3_printf1ln(debug_msg, "\tMIME header (line 1) expected, ignoring %s", special_quoting(line));
    } while(!found);
    // wait/check for content length:
    do  {
        if(!(line = http_get_line())) {
            debug1_println("http_stream_get_jpeg(): error reading MIME header (content length)");
            return HTTP_GET_ERROR_RETURN;
        }
        found = strncmp(line, mark2, sizeof(mark2)-1) == 0;
        if(! found)
            debug3_printf1ln(debug_msg, "\tContent length expected, ignoring %s", special_quoting(line));
    } while(!found);
    // parse length of MIME content:
    announced_size = atoi(line + sizeof(mark2) - 1);
    debug3_printf1ln(debug_msg, "\texpected content: %d bytes", announced_size);
    // wait for single hex number, content length:
    do  {
        if(!(line = http_get_line())) {
            debug1_println("http_stream_get_jpeg(): error reading MIME header (hex length)");
            return HTTP_GET_ERROR_RETURN;
        } else
            debug3_printf1ln(debug_msg, "\tSkipping: %s", special_quoting(line));
    } while(line[0] == 0);  // while line is empty

    // read the image itself:
    bytes_read = http_get_binary(buffer, min(announced_size, bufferSize));
    if(bytes_read != min(announced_size, bufferSize)) {
        debug1_printf2ln(debug_msg, "http_stream_get_jpeg(): error reading binary image data: %d bytes read instead of expected %d", bytes_read, announced_size);
        return HTTP_GET_ERROR_RETURN;
    } else
        debug3_printf1ln(debug_msg, "\tsuccessfully read %d bytes if JPEG data(?)", bytes_read);
    
    // is the buffer sufficient?
    if(bytes_read < announced_size) {
        debug1_printf2ln(debug_msg, "http_stream_get_jpeg(): error buffer of %d bytes is exceeded by result of %d bytes", bufferSize, announced_size);
        // try to read the remaining data to enable the processing of following MIME parts:
        size_t to_skip = announced_size - bytes_read;
        while(to_skip > 0)
            to_skip -= http_get_binary(buffer, max(to_skip, bufferSize));
        return HTTP_GET_ERROR_RETURN;
    }
    
    // check for empty line (after the image data):
    if(!(line = http_get_line())) {
        debug1_println("http_stream_get_jpeg(): error reading MIME (separator, empty line after)");
        return HTTP_GET_ERROR_RETURN;
    }
    if(*line) {
        debug1_printf1ln(debug_msg, "http_stream_get_jpeg(): unexpected data following binary data (empty line expected): '%s'", line);
        return HTTP_GET_ERROR_RETURN;
    }
    
    debug2_printf1ln(debug_msg, "\treturning %d", bytes_read);
    return bytes_read;
}

//====================================================================================
// "process" reading from the network to the buffer(s)
//====================================================================================
void read_process(void) {
    static short current_buffer = 0;
    
    while(true) {
        // wait for the buffer to become available & "allocate" it:
        PROFILED_DO(profile_reader_wait, jpegBuffers[current_buffer].allocate(BUFFER_FREE, BUFFER_READING));

        // read to the buffer (repeating til success)
        PROFILED_START(profile_read_network);
        while((jpegBuffers[current_buffer].usedSize = http_stream_get_jpeg(jpegBuffers[current_buffer].data, jpegBuffers[current_buffer].size)) < 1)
            yield();
        PROFILED_END(profile_read_network);
        // "free" the filled buffer
        jpegBuffers[current_buffer].state = BUFFER_FILLED;
        // switch to next buffer
        current_buffer = (current_buffer + 1) % NUM_BUFFERS;
    }
}

//====================================================================================
// "process" decoding jpeg from buffer(s) & writing the "result" to the display:
//====================================================================================
void display_process(void) {
#ifdef CAPTURE_DIR
    static int imgcount = 0;
    static char filename[100];
#endif
    static short current_buffer = 0;
    
    while(true) {
        // wait for the buffer to become available & "allocate" it:
        PROFILED_DO(profile_display_wait, jpegBuffers[current_buffer].allocate(BUFFER_FILLED, BUFFER_DISPLAYING));
#ifdef SCREEN_DEBUG
    GO.lcd.fillScreen(TFT_BLACK);
    GO.lcd.setCursor(0,0);
#endif
        // "paint" the image to the display
        PROFILED_DO(profile_paint, GO.lcd.drawJpg(jpegBuffers[current_buffer].data, jpegBuffers[current_buffer].usedSize, 0, 0));

#ifdef CAPTURE_DIR
        // dump to file:
        snprintf(filename, sizeof(filename)-1, CAPTURE_DIR "/stream-%04d.jpg", imgcount++);
        PROFILED_DO(profile_write_SD, dumpToFile(SD, filename, jpegBuffers[current_buffer].data, jpegBuffers[current_buffer].usedSize));
#endif
        // "free" the processed buffer
        jpegBuffers[current_buffer].state = BUFFER_FREE;
        // switch to next buffer
        current_buffer = (current_buffer + 1) % NUM_BUFFERS;
    }
}

//====================================================================================
//                                    Setup
//====================================================================================

TaskHandle_t  ReaderTask, DisplayTask;

void setup(void)
{
    GO.begin(115200);
    delay(10);
    Serial.println("ODroid Go OV2640 (capture) example");
    // fix speaker "ticking" (assuming the speaker is not used at all!!!)
    // source: https://forum.odroid.com/viewtopic.php?f=160&t=33336#p245177
    // pinMode(25, OUTPUT); // already done by GO.begin() => Speaker.begin()
    digitalWrite(25, LOW);

    GO.lcd.clear();
    GO.lcd.setTextSize(2);
    GO.lcd.println("ODroid Go OV2640 (capture) example");

#ifdef CAPTURE_DIR
    if(!SD.begin()) {
        debug1_println("*** SD card initialisation failed! ***");
        while (1) yield(); // Stay here twiddling thumbs waiting
    }
    File dir = SD.open(CAPTURE_DIR);
    if(! dir) {
        debug1_println("directory " CAPTURE_DIR " can't be opened; trying to create");
        if(! SD.mkdir(CAPTURE_DIR)) {
            debug1_println("*** error creating directory " CAPTURE_DIR " ***");
            while (1) yield(); // Stay here twiddling thumbs waiting
        }
        if(!(dir = SD.open(CAPTURE_DIR))) {
            debug1_println("*** error opening just created directory " CAPTURE_DIR " ***");
            while (1) yield(); // Stay here twiddling thumbs waiting
        }
        debug2_println("capture directory " CAPTURE_DIR " created");
    } else if(!dir.isDirectory()) {
        debug1_println("*** " CAPTURE_DIR " is present, but no directory! ***");
        while (1) yield(); // Stay here twiddling thumbs waiting
    }            
#endif

    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWD);
    while(wifiMulti.run() != WL_CONNECTED) {
        debug1_println("waiting for WiFi " WIFI_SSID);
        delay(300);
    }

    // start worker tasks:
    xTaskCreate((TaskFunction_t) read_process,    "reader",  HEAP_READER,  NULL, 1, &ReaderTask);
    xTaskCreate((TaskFunction_t) display_process, "display", HEAP_DISPLAY, NULL, 1, &DisplayTask);

    debug1_println("Initialisation done.");
    delay(500);
}

//====================================================================================
//                                    Loop
//====================================================================================
void loop(void)
{   static int report_next = PROFILING_STEPPING;

#ifdef PROFILING
    if(profile_paint.n_runs >= report_next) {
        profile_stream_open.tabular_head();
        profile_stream_open.tabular_print();
        profile_reader_wait.tabular_print();
        profile_read_network.tabular_print();
        profile_display_wait.tabular_print();
#ifdef CAPTURE_DIR
        profile_write_SD.tabular_print();
#endif CAPTURE_DIR
        profile_paint.tabular_print();
        
        report_next += PROFILING_STEPPING;
    }
#endif PROFILING
    delay(1000);
}
//====================================================================================
