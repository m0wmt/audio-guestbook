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
AudioPlaySdWav          playWav1;       // Play 44.1kHz 16-bit PCM .WAV files
AudioInputI2S           audioInput;     // I2S input from microphone on Teensy 4.0 Audio shield
AudioMixer4             mixer;          // Allows merging several inputs to same output
AudioRecordQueue        queue1;         // Create an audio buffer in memory before saving to SD
AudioSynthWaveform      waveform1;      // To create the "beep" sound effect
AudioOutputI2S          audioOutput;    // I2S output to Speaker Out on Teensy 4.0 Audio shield
AudioConnection         patchCord1(waveform1, 0, mixer, 0);
AudioConnection         patchCord2(playWav1, 0, mixer, 1);
AudioConnection         patchCord3(mixer, 0, audioOutput, 0); // mixer output to speaker (L)
AudioConnection         patchCord4(mixer, 0, audioOutput, 1); // mixer output to speaker (R)
AudioConnection         patchCord5(audioInput, 0, queue1, 0); // mic input to queue (L)
AudioControlSGTL5000    audioShield;

// Keep track of current state of the device
enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;

float beep_volume = 0.9f; // not too loud

int blinkLed = 500;         // Blink led every n miliseconds
int ledState = LOW;         // LED state, LOW or HIGH
static int oneSecond = 1000;
char filename[15];          // Filename to save audio recording on SD card
File frec;                  // The file object itself
unsigned long recByteSaved = 0L;

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
static void startRecording(void);
static void continueRecording(void);
static void stopRecording(void);
static void writeOutHeader(void);

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

    Serial.println("Serial set up correctly");
    Serial.printf("Audio block set to %d samples\n",AUDIO_BLOCK_SAMPLES);
  
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

    // Comment these out if not using the audio adaptor board.
    audioShield.enable();
    audioShield.volume(0.6);

    // Define which input on the audio shield to use (AUDIO_INPUT_LINEIN / AUDIO_INPUT_MIC)
    audioShield.inputSelect(AUDIO_INPUT_MIC);
    audioShield.micGain(40);  // gain in dB from 0 to 63

    mixer.gain(0, 1.0f);
    mixer.gain(1, 1.0f);
    
    // Play a beep to indicate system is online
    waveform1.begin(beep_volume, 440, WAVEFORM_SINE);
    wait(1000);
    waveform1.amplitude(0);
    delay(1000);

    // Serial.println("Attempt to play beeps!");
    // // Play a beep to indicate system is online
    // Serial.println("End beep");
    // end_Beep();
    // delay(1000);
    // Serial.println("Two tone beep");
    // two_tone_Beep();
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

    // Reset the maximum reported by AudioMemoryUsageMax
    AudioMemoryUsageMaxReset();

    digitalWrite(LED_BUILTIN, LOW);   // Turn LED off
}  

/**
 * @brief Main loop
 */
void loop() {
    // Read the buttons - can we move these to an interrupt?
    buttonRecord.update();
    buttonPress.update();

    switch(mode) {
        case Mode::Ready: // to make compiler happy
        break;  

        case Mode::Initialising: // to make compiler happy
        break;  

        case Mode::Prompting: // to make compiler happy
        break;  

        case Mode::Recording:
            continueRecording();
        break;

        case Mode::Playing: // to make compiler happy
        break;  
    }

    // Falling edge occurs when the handset is lifted --> GPO 706 telephone
    if (buttonRecord.fallingEdge()) {
        #if DEBUG
        Serial.println("Handset lifted");
        #endif
        delay(1000);
        startRecording();
    } else if (buttonRecord.risingEdge()) {  // Handset is replaced
        #if DEBUG
        Serial.println("Handset replaced");
        #endif
        delay(1000);
        stopRecording();
        end_Beep();
    }

    // Falling edge occurs when the PRESS button is lifted --> GPO 706 telephone
    if (buttonPress.fallingEdge()) {
        #if DEBUG
        Serial.println("PRESS button pressed");
        #endif
        delay(1000);
        startRecording();
    } else if (buttonPress.risingEdge()) {  // PRESS button is released
        #if DEBUG
        Serial.println("PRESS button released");
        #endif
        delay(1000);
        stopRecording();
        #if DEBUG
        Serial.print("Max number of blocks used by Audio were: ");
        Serial.println(AudioMemoryUsageMax());
        #endif
    }

    blink_led();
//    print_time();
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

static void startRecording(void) {
  // Find the first available file number
//  for (uint8_t i=0; i<9999; i++) { // BUGFIX uint8_t overflows if it reaches 255  
  for (uint16_t i=0; i<9999; i++) {   
    // Format the counter as a five-digit number with leading zeroes, followed by file extension
    snprintf(filename, 11, " %05d.wav", i);
    // Create if does not exist, do not open existing, write, sync after write
    if (!SD.exists(filename)) {
      break;
    }
  }
  frec = SD.open(filename, FILE_WRITE);
  Serial.println("Creating file.");
  if(frec) {
    Serial.print("Recording to ");
    Serial.println(filename);
    queue1.begin();
    mode = Mode::Recording; print_mode();
    recByteSaved = 0L;
  }
  else {
    Serial.println("Couldn't open file to record!");
  }
}

static void continueRecording(void) {
#define NBLOX 16  
  // Check if there is data in the queue
  if (queue1.available() >= NBLOX) {
    byte buffer[NBLOX*AUDIO_BLOCK_SAMPLES*sizeof(int16_t)];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    for (int i=0;i<NBLOX;i++)
    {
      memcpy(buffer+i*AUDIO_BLOCK_SAMPLES*sizeof(int16_t), queue1.readBuffer(), AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
      queue1.freeBuffer();
    }
    // Write all 512 bytes to the SD card
    frec.write(buffer, sizeof buffer);
    recByteSaved += sizeof buffer;
  }  
}

static void stopRecording(void) {
  Serial.println("stopRecording");
  // Stop adding any new data to the queue
  queue1.end();
  // Flush all existing remaining data from the queue
  while (queue1.available() > 0) {
    // Save to open file
    frec.write((byte*)queue1.readBuffer(), AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
    queue1.freeBuffer();
    recByteSaved += AUDIO_BLOCK_SAMPLES*sizeof(int16_t);
    Serial.println("Flushed audio to file");
  }
  writeOutHeader();
  // Close the file
  frec.close();
  Serial.println("Closed file");
  mode = Mode::Ready; print_mode();
}


static void writeOutHeader(void) { // update WAV header with final filesize/datasize
    // variables for writing to WAV file
    unsigned long ChunkSize = 0L;
    unsigned long Subchunk1Size = 16;
    unsigned int AudioFormat = 1;
    unsigned int numChannels = 1;
    unsigned long sampleRate = 44100;
    unsigned int bitsPerSample = 16;
    unsigned long byteRate = sampleRate*numChannels*(bitsPerSample/8);// samplerate x channels x (bitspersample / 8)
    unsigned int blockAlign = numChannels*bitsPerSample/8;
    unsigned long Subchunk2Size = 0L;
    byte byte1, byte2, byte3, byte4;

  //  NumSamples = (recByteSaved*8)/bitsPerSample/numChannels;
  //  Subchunk2Size = NumSamples*numChannels*bitsPerSample/8; // number of samples x number of channels x number of bytes per sample
    Subchunk2Size = recByteSaved - 42; // because we didn't make space for the header to start with! Lose 21 samples...
    ChunkSize = Subchunk2Size + 34; // was 36;
    frec.seek(0);
    frec.write("RIFF");
    byte1 = ChunkSize & 0xff;
    byte2 = (ChunkSize >> 8) & 0xff;
    byte3 = (ChunkSize >> 16) & 0xff;
    byte4 = (ChunkSize >> 24) & 0xff;  
    frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
    frec.write("WAVE");
    frec.write("fmt ");
    byte1 = Subchunk1Size & 0xff;
    byte2 = (Subchunk1Size >> 8) & 0xff;
    byte3 = (Subchunk1Size >> 16) & 0xff;
    byte4 = (Subchunk1Size >> 24) & 0xff;  
    frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
    byte1 = AudioFormat & 0xff;
    byte2 = (AudioFormat >> 8) & 0xff;
    frec.write(byte1);  frec.write(byte2); 
    byte1 = numChannels & 0xff;
    byte2 = (numChannels >> 8) & 0xff;
    frec.write(byte1);  frec.write(byte2); 
    byte1 = sampleRate & 0xff;
    byte2 = (sampleRate >> 8) & 0xff;
    byte3 = (sampleRate >> 16) & 0xff;
    byte4 = (sampleRate >> 24) & 0xff;  
    frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
    byte1 = byteRate & 0xff;
    byte2 = (byteRate >> 8) & 0xff;
    byte3 = (byteRate >> 16) & 0xff;
    byte4 = (byteRate >> 24) & 0xff;  
    frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
    byte1 = blockAlign & 0xff;
    byte2 = (blockAlign >> 8) & 0xff;
    frec.write(byte1);  frec.write(byte2); 
    byte1 = bitsPerSample & 0xff;
    byte2 = (bitsPerSample >> 8) & 0xff;
    frec.write(byte1);  frec.write(byte2); 
    frec.write("data");
    byte1 = Subchunk2Size & 0xff;
    byte2 = (Subchunk2Size >> 8) & 0xff;
    byte3 = (Subchunk2Size >> 16) & 0xff;
    byte4 = (Subchunk2Size >> 24) & 0xff;  
    frec.write(byte1);  frec.write(byte2);  frec.write(byte3);  frec.write(byte4);
    frec.close();
    Serial.println("header written"); 
    Serial.print("Subchunk2: "); 
    Serial.println(Subchunk2Size); 
  }