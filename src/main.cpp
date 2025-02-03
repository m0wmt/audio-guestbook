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
#include <Bounce.h>

#include "play_sd_wav.h"

// Defines
// Use Teensy 4.1 SD card, teensy forum thinks it is faster than the audio board SD card interface.
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

#define HOOK_PIN 33         // Handset switch
#define PRESS_PIN 34        // PRESS switch

// Globals
// All audio should be WAV files (44.1kHz 16-bit PCM)
AudioPlaySdWav           playWav1;      
AudioOutputI2S           audioOutput;   // I2S interface to Speaker/Line Out on Teensy 4.0 Audio shield
AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;
AudioSynthWaveform       waveform1;     // To create the "beep" sfx


// Use long 40ms debounce time on switches
Bounce buttonRecord = Bounce(HOOK_PIN, 40);
Bounce buttonPress = Bounce(PRESS_PIN, 40);

/* Function prototypes */
static void play_file(const char *filename);

/**
 * @brief Program setup
 */
void setup() {
    while (!Serial) {
    // Wait for serial to start!
    } 

    Serial.println("Setting up audio and SD card");

    // Configure the input pins
    pinMode(HOOK_PIN, INPUT_PULLUP);
    pinMode(PRESS_PIN, INPUT_PULLUP);

    // Reset the maximum reported by AudioMemoryUsageMax
    AudioMemoryUsageMaxReset();

    // Audio connections require memory to work. The numberBlocks input specifies how much memory 
    // to reserve for audio data. Each block holds 128 audio samples, or approx 2.9 ms of sound. 
    AudioMemory(8);

    // Comment these out if not using the audio adaptor board.
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

    // filenames are always uppercase 8.3 format
    //Serial.println("Starting to play example .wav files");
    //play_file("SDTEST1.WAV");  
    //delay(500);
    //play_file("SDTEST2.WAV");
    //delay(500);
    //play_file("SDTEST3.WAV");
    //delay(500);
    // play_file("SDTEST4.WAV");
    // delay(1500);  
    //Serial.println("Example .wav files finished playing");

    // Get the maximum number of blocks that have ever been used
    Serial.print("Max number of blocks used by Audio were: ");
    Serial.println(AudioMemoryUsageMax());
}  

/**
 * @brief Main loop
 */
void loop() {
    // Read the buttons - can we move these to an interrupt?
    buttonRecord.update();
    buttonPress.update();

    // Falling edge occurs when the handset is lifted --> GPO 706 telephone
    if (buttonRecord.fallingEdge()) {
        Serial.println("Handset lifted");
        delay(1000);
    } else if (buttonRecord.risingEdge()) {  // Handset is replaced
        Serial.println("Handset replaced");
        delay(1000);
    }

    // Falling edge occurs when the PRESS button is lifted --> GPO 706 telephone
    if (buttonPress.fallingEdge()) {
        Serial.println("PRESS button pressed");
        delay(1000);
    } else if (buttonPress.risingEdge()) {  // PRESS button is released
        Serial.println("PRESS button released");
        delay(1000);
    }

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

