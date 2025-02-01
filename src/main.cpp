/**
 * Audio Guestbook, based on original by Playful Technology (2022) https://github.com/playfultechnology/audio-guestbook.
 * 
 * This program is designed to record messages so a micro SD card using an old Rotary telephone at an upcoming wedding.
 * 
 * Requirements are available in the README.md file.
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Audio.h>

#include "play_sd_wav.h"

// GLOBALS

/* Variables to obtain free RAM */
extern unsigned long _heap_start;
extern unsigned long _heap_end;
extern char *__brkval;

AudioPlaySdWav           playWav1;
AudioOutputI2S           audioOutput;
AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;

// Use Teensy 4.1 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used



/* Function prototypes */
static int free_ram(void);
static void play_file(const char *filename);

/**
 * @brief Program setup
 */
void setup() {
    while (!Serial) {
    // Wait for serial to start!
    } 

    Serial.print("Free RAM = ");
    Serial.println(free_ram());

    Serial.println("Setting up audio and SD card");
    // Audio connections require memory to work.  For more
    // detailed information, see the MemoryAndCpuUsage example
    AudioMemory(8);

    // Comment these out if not using the audio adaptor board.
    // This may wait forever if the SDA & SCL pins lack
    // pullup resistors
    sgtl5000_1.enable();
    sgtl5000_1.volume(0.5);

    SPI.setMOSI(SDCARD_MOSI_PIN);
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(SDCARD_CS_PIN))) {
        // stop here, but print a message repetitively
        while (1) {
            Serial.println("Unable to access the SD card");
            delay(500);
        }
    }

    Serial.println("Starting to play example .wav files");
    play_file("SDTEST1.WAV");  // filenames are always uppercase 8.3 format
    delay(500);
    play_file("SDTEST2.WAV");
    delay(500);
    play_file("SDTEST3.WAV");
    delay(500);
    play_file("SDTEST4.WAV");
    delay(1500);  
    Serial.println("Example .wav files finished playing");
}  

/**
 * @brief Main loop
 */
void loop() {
}


/**
 * @brief Obtain free RAM
 * 
 * @returns Available RAM
 */
static int free_ram(void) {
    return (char *)&_heap_end - __brkval;
}

/**
 * @brief Play audio .wav file
 */
static void play_file(const char *filename)
{
    Serial.print("Playing file: ");
    Serial.println(filename);

    // Start playing the file.  This sketch continues to
    // run while the file plays.
    playWav1.play(filename);

    // A brief delay for the library read WAV info
    delay(25);

    // Simply wait for the file to finish playing.
    while (playWav1.isPlaying()) {
    }
}

