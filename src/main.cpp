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
#include <TimeLib.h>

#include "play_sd_wav.h"

#define DEBUG true       // do/do not use serial

// Defines
// Use Teensy 4.1 SD card, teensy forum thinks it is faster than the audio board SD card interface.
#define SDCARD_CS_PIN    BUILTIN_SDCARD
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

#define HANDSET_PIN 41      // Handset switch
#define PRESS_PIN 40        // PRESS switch

// Globals
// All audio should be WAV files (44.1kHz 16-bit PCM)
AudioPlaySdWav          playWav1;      
AudioOutputI2S          audioOutput;    // I2S interface to Speaker/Line Out on Teensy 4.0 Audio shield
AudioMixer4             mixer;          // Allows merging several inputs to same output
AudioRecordQueue        queue1;         // Create an audio buffer in memory before saving to SD
AudioSynthWaveform      waveform1;      // To create the "beep" sound effect
AudioConnection         patchCord1(waveform1, 0, mixer, 0);
AudioConnection         patchCord2(playWav1, 0, mixer, 1);
AudioConnection         patchCord3(mixer, 0, audioOutput, 0); // mixer output to speaker (L)
AudioConnection         patchCord4(mixer, 0, audioOutput, 1); // mixer output to speaker (R)
AudioConnection         patchCord5(audioOutput, 0, queue1, 1); // mic input to queue (L)
AudioControlSGTL5000    audioShield;

// Keep track of current state of the device
enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;

float beep_volume = 0.9f; // not too loud

int blinkLed = 500;         // Blink led every n miliseconds
int ledState = LOW;         // LED state, LOW or HIGH
static int oneSecond = 1000;

// Use long 40ms debounce time on switches
Bounce buttonRecord = Bounce(HANDSET_PIN, 40);
Bounce buttonPress = Bounce(PRESS_PIN, 40);

/* Function prototypes */
static void play_file(const char *filename);
static void wait(unsigned int milliseconds);
static void end_Beep(void);
static void two_tone_Beep(void);
static void blink_led(void);
static time_t getTeensy3Time(void);
static void printDigits(int digits);
static void digitalClockDisplay(void);
static void print_time(void);

#if DEBUG
static void print_mode(void);           // for debugging only
#endif

/**
 * @brief Program setup
 */
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);     // Orange LED on board
    digitalWrite(LED_BUILTIN, HIGH);  // Glowing whilst running through setup code
    
    #if DEBUG
    while (!Serial) {
    // Wait for serial to start!
    } 

    print_mode();

    Serial.println("Setting up audio and SD card");
    #endif

    setSyncProvider(getTeensy3Time);   // the function to get the time from the RTC
    if (timeStatus() != timeSet) 
        Serial.println("Unable to sync with the RTC");
    else
        Serial.println("RTC has set the system time");     

    // Configure the input pins
    pinMode(HANDSET_PIN, INPUT_PULLUP);
    pinMode(PRESS_PIN, INPUT_PULLUP);

    // Reset the maximum reported by AudioMemoryUsageMax
    AudioMemoryUsageMaxReset();

    // Audio connections require memory to work. The numberBlocks input specifies how much memory 
    // to reserve for audio data. Each block holds 128 audio samples, or approx 2.9 ms of sound. 
    AudioMemory(60);

    mixer.gain(0, 1.0f);
    mixer.gain(1, 1.0f);
    
    // Comment these out if not using the audio adaptor board.
    audioShield.enable();
    audioShield.volume(0.6);
    // audioShield.dacVolume(1.0);
    // audioShield.dacVolumeRamp();

    // Define which input on the audio shield to use (AUDIO_INPUT_LINEIN / AUDIO_INPUT_MIC)
    audioShield.inputSelect(AUDIO_INPUT_MIC);

    // Play a beep to indicate system is online
    waveform1.begin(beep_volume, 440, WAVEFORM_SINE);
    wait(1000);
    waveform1.amplitude(0);
    delay(1000);

    // Serial.println("Attempt to play beeps!");
    // // Play a beep to indicate system is online
    Serial.println("End beep");
    end_Beep();
    delay(1000);
    Serial.println("Two tone beep");
    two_tone_Beep();
    // Serial.println("Beeps possibly played!");

    // Init SD CARD
    SPI.setMOSI(SDCARD_MOSI_PIN);
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(SDCARD_CS_PIN))) {
        // stop here, and print a message repetitively
        while (1) {
            #if DEBUG
            Serial.println("Unable to access the SD card");
            #endif
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
    #if DEBUG
    Serial.print("Max number of blocks used by Audio were: ");
    Serial.println(AudioMemoryUsageMax());
    #endif

    mode = Mode::Ready; 
    
    #if DEBUG
    print_mode();
    #endif

    digitalWrite(LED_BUILTIN, LOW);   // Turn LED off
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
        #if DEBUG
        Serial.println("Handset lifted");
        #endif
        delay(1000);
    } else if (buttonRecord.risingEdge()) {  // Handset is replaced
        #if DEBUG
        Serial.println("Handset replaced");
        #endif
        delay(1000);
    }

    // Falling edge occurs when the PRESS button is lifted --> GPO 706 telephone
    if (buttonPress.fallingEdge()) {
        #if DEBUG
        Serial.println("PRESS button pressed");
        #endif
        delay(1000);
    } else if (buttonPress.risingEdge()) {  // PRESS button is released
        #if DEBUG
        Serial.println("PRESS button released");
        #endif
        delay(1000);
    }

    blink_led();
    print_time();
}


/**
 * @brief Play audio .wav file
 */
static void play_file(const char *filename)
{
    #if DEBUG
    Serial.print("Playing file: ");
    Serial.println(filename);
    #endif

    // Start playing the file.  This sketch continues to
    // run while the file plays.
    playWav1.play(filename);

    // A brief delay for the library read WAV info
    delay(25);

    // Simply wait for the file to finish playing.
    while (playWav1.isPlaying()) {
    }
}

#if DEBUG
static void print_mode(void) { // for debugging only
    Serial.print("Mode switched to: ");
    // Initialising, Ready, Prompting, Recording, Playing
    if(mode == Mode::Ready)           Serial.println(" Ready");
    else if(mode == Mode::Prompting)  Serial.println(" Prompting");
    else if(mode == Mode::Recording)  Serial.println(" Recording");
    else if(mode == Mode::Playing)    Serial.println(" Playing");
    else if(mode == Mode::Initialising)  Serial.println(" Initialising");
    else Serial.println(" Undefined");
}
#endif

// Non-blocking delay, which pauses execution of main program logic,
// while still listening for input 
static void wait(unsigned int milliseconds) {
  elapsedMillis msec=0;

  while (msec <= milliseconds) {
    buttonRecord.update();
//    buttonPlay.update();
    if (buttonRecord.fallingEdge()) {
        #if DEBUG
        Serial.println("Button (pin 0) Press");
        #endif
    }
//   if (buttonPlay.fallingEdge()) Serial.println("Button (pin 1) Press");
    if (buttonRecord.risingEdge()) {
        #if DEBUG
        Serial.println("Button (pin 0) Release");
        #endif
    }
//    if (buttonPlay.risingEdge()) Serial.println("Button (pin 1) Release");
  }
}

static void end_Beep(void) {
	waveform1.frequency(523.25);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
	wait(250);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
	wait(250);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
	wait(250);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
}

static void two_tone_Beep(void) {
	waveform1.frequency(523.25);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
	waveform1.frequency(375.0);
	wait(250);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
	waveform1.frequency(523.25);
	wait(250);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
	waveform1.frequency(375.0);
	wait(250);
	waveform1.amplitude(beep_volume);
	wait(250);
	waveform1.amplitude(0);
}

static void blink_led(void) {
    static unsigned long previousMillis;
    unsigned long timeNow = millis();

    if (timeNow - previousMillis >= blinkLed) {
        previousMillis = timeNow;
        if (ledState == LOW) {
            ledState = HIGH;
        } else {
            ledState = LOW;
        }

        digitalWrite(LED_BUILTIN, ledState);
    }
}

static time_t getTeensy3Time(void) {
    return Teensy3Clock.get();
}

static void printDigits(int digits) {  
    // utility function for digital clock display: prints preceding colon and leading 0
    Serial.print(":");
    if(digits < 10)
        Serial.print('0');

    Serial.print(digits);
}

static void digitalClockDisplay(void) {
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(" ");
    Serial.print(day());
    Serial.print(" ");
    Serial.print(month());
    Serial.print(" ");
    Serial.print(year()); 
    Serial.println(); 
}

static void print_time(void) {
    static unsigned long previousTimeInMillis;
    unsigned long timeNow = millis();

    if (timeNow - previousTimeInMillis >= oneSecond) {
        previousTimeInMillis = timeNow;

        digitalClockDisplay();
    }
}