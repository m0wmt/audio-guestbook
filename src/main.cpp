/**
 * Audio Guestbook, based on original by Playful Technology (2022)
 * https://github.com/playfultechnology/audio-guestbook - see copyright below.
 *
 * This program is designed to record messages so a micro SD card using an old GPO 706
 * rotary telephone, a Teensy 4.1, and a Teensy Audio Shield for an upcoming wedding.
 *
 * Requirements are available in the README.md file, a journal of my journey is available in the appropriately named
 * JOURNAL.md file.
 *
 * Modified AUDIO_BLOCK_SAMPLES to 256 in
 * /home/#########/.platformio/packages/framework-arduinoteensy/cores/teensy4/AudioStream.h as suggested by the audio
 * guestbook thread on the PRJC forum.
 */

/**
 * Audio Guestbook, Copyright (c) 2022 Playful Technology
 *
 * Tested using a Teensy 4.0 with Teensy Audio Shield, although should work
 * with minor modifications on other similar hardware
 *
 * When handset is lifted, a pre-recorded greeting message is played, followed by a tone.
 * Then, recording starts, and continues until the handset is replaced.
 * Playback button allows all messages currently saved on SD card through earpiece
 *
 * Files are saved on SD card as 44.1kHz, 16-bit, mono signed integer RAW audio format
 * --> changed this to WAV recording, DD4WH 2022_07_31
 * --> added MTP support, which enables copying WAV files from the SD card via the USB connection, DD4WH 2022_08_01
 *
 * Frank DD4WH, August 1st 2022
 * for a DBP 611 telephone (closed contact when handheld is lifted) & with recording to WAV file
 * contact for switch button 0 is closed when handheld is lifted
 *
 * GNU GPL v3.0 license
 *
 */

#include <Arduino.h>
#include <Audio.h>
#include <Bounce.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <TimeLib.h>
#include <Wire.h>
#include "play_sd_wav.h"

/* Defines */

// Use Teensy 4.1 SD card instead of the audio boards SD card interface.
#define SDCARD_CS_PIN BUILTIN_SDCARD
#define SDCARD_MOSI_PIN 11
#define SDCARD_SCK_PIN 13

#define HANDSET_PIN 41      // Handset switch
#define PRESS_PIN 40        // PRESS switch
#define WARNING_DELAY 1000  // Play a warning sound every 'n' milliseconds
#define LED_BLINK_DELAY 500 // Blink LED every 'n' milliseconds

static const uint8_t morse_time_unit = 80;         // Morse code time unit, length of a dot is 1 time unit
static const uint32_t max_recording_time = 30'000; // Recording time limit (milliseconds)
// 60000 = 1 min
// 120000 =  2 mins
// 240000 = 4 mins

/* Globals */
AudioPlaySdWav wave_file;              // Play 44.1kHz 16-bit PCM .WAV files
AudioInputI2S audio_input;              // I2S input from microphone on Teensy 4.0 Audio shield
AudioMixer4 mixer;                     // Allows merging several inputs to same output
AudioRecordQueue queue1;               // Create an audio buffer in memory before saving to SD
AudioSynthWaveform synth_waveform;     // To create the "beep" sound effect
AudioSynthWaveform synth_waveform_350; // To create UK dial tone
AudioSynthWaveform synth_waveform_450; // To create UK dial tone
AudioOutputI2S audio_output;           // I2S output to Speaker Out on Teensy 4.0 Audio shield
AudioConnection patchCord1(synth_waveform, 0, mixer, 0);
AudioConnection patchCord2(wave_file, 0, mixer, 1);
AudioConnection patchCord3(mixer, 0, audio_output, 0); // mixer output to speaker (L)
AudioConnection patchCord4(mixer, 0, audio_output, 1); // mixer output to speaker (R)
AudioConnection patchCord5(synth_waveform_350, 0, mixer, 2);
AudioConnection patchCord6(synth_waveform_450, 0, mixer, 3);
AudioConnection patchCord7(audio_input, 0, queue1, 0); // mic input to queue (L)
AudioControlSGTL5000 audio_shield;

typedef enum { // Keep track of current state of the device
    ERROR,
    INITIALISING,
    READY,
    RECORDMESSAGEPROMPT,
    RECORDING,
    PLAYING
} button_mode_t;
button_mode_t mode = INITIALISING;

typedef enum { // Keep track of dial tone state
    OFF,
    ON
} dial_tone_state_t;
dial_tone_state_t dial_tone = OFF;

float beep_volume = 0.9f; // not too loud
int led_state = LOW;      // LED state, LOW or HIGH
static int one_second = 1000;
char filename[15]; // Filename to save audio recording on SD card
File file_object;  // The file object itself
unsigned long record_bytes_saved = 0L;
uint32_t wait_start = 0;
elapsedMillis recording_timer = 0; // Recording timer to prevent long messages

// Debounce on switches
Bounce phone_handset = Bounce(HANDSET_PIN, 40);
Bounce press_button = Bounce(PRESS_PIN, 40);

/* Function prototypes */
static void play_file(const char *filename);
static void wait(unsigned int milliseconds);
static void end_beep(void);
static void error(void);
static void sd_card_error(void);
static void blink_led(void);
static time_t get_teensy_three_time(void);
static void print_digits(int digits);
static void digital_clock_display(void);
static void print_time(void);
static void sound_warning(void);
static void start_recording(void);
static void continue_recording(void);
static void stop_recording(void);
static void write_out_wav_header(void);
static void print_mode(void); // for debugging only
static void dialing_tone(dial_tone_state_t on_or_off);

/**
 * @brief Program setup.
 */
void setup() {
    bool b_sd_card = false; // SD card present or not

    pinMode(LED_BUILTIN, OUTPUT);    // Orange LED on board
    digitalWrite(LED_BUILTIN, HIGH); // Glowing whilst running through setup code

    // while (!Serial) {
    //     // Wait for serial to start!
    // }

    delay(1000);

    Serial.println("Serial set up correctly");

    Serial.printf("Audio block set to %d samples\n", AUDIO_BLOCK_SAMPLES);

    print_mode();

    Serial.println("Setting up audio and SD card");

    setSyncProvider(get_teensy_three_time); // the function to get the time from the RTC
    if (timeStatus() != timeSet)
        Serial.println("Unable to sync with the RTC");
    else
        Serial.println("RTC has set the system time");

    // Configure the input pins
    pinMode(HANDSET_PIN, INPUT_PULLUP);
    pinMode(PRESS_PIN, INPUT_PULLUP);

    // Reset the maximum reported by AudioMemoryUsageMax
    AudioMemoryUsageMaxReset();

    // Audio connections require memory to work. The numberBlocks input
    // specifies how much memory to reserve for audio data. Each block holds 256
    // audio samples, or approx 5.8 ms of sound.
    AudioMemory(100);

    // Comment these out if not using the audio adaptor board.
    audio_shield.enable();
    audio_shield.volume(0.6);

    // Define which input on the audio shield to use (AUDIO_INPUT_LINEIN /
    // AUDIO_INPUT_MIC)
    audio_shield.inputSelect(AUDIO_INPUT_MIC);
    audio_shield.micGain(40); // gain in dB from 0 to 63

    mixer.gain(0, 1.0f);
    mixer.gain(1, 1.0f);
    mixer.gain(2, 1.0f);
    mixer.gain(3, 1.0f);

    // Play a sound to indicate system is online
    dialing_tone(ON);

    // Init SD CARD
    SPI.setMOSI(SDCARD_MOSI_PIN);
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(SDCARD_CS_PIN))) {
        // Stop here, and print a message repetitively, this will cause the LED to stay lit (not that anyone can see
        // this!) and the beep tone to carry on playing to indicate an error.
        while (!b_sd_card) {
            delay(500);
            sd_card_error();
            if (SD.begin(SDCARD_CS_PIN)) {
                b_sd_card = true;
                Serial.println("SD card present");
            }
        }
    } else {
        b_sd_card = true;
        Serial.println("SD card present");
    }

    delay(3000); // So we get to hear that the system is working!

    // filenames are always uppercase 8.3 format
    // Serial.println("Starting to play example .wav files");
    // play_file("SDTEST1.WAV");
    // delay(500);
    // play_file("SDTEST2.WAV");
    // delay(500);
    // play_file("SDTEST3.WAV");
    // delay(500);
    // play_file("SDTEST4.WAV");
    // delay(1500);
    // Serial.println("Example .wav files finished playing");

    // Get the maximum number of blocks that have ever been used
    Serial.print("Max number of blocks used by Audio were: ");
    Serial.println(AudioMemoryUsageMax());

    mode = READY;
    print_mode();

    dialing_tone(OFF);

    // Reset the maximum reported by AudioMemoryUsageMax
    AudioMemoryUsageMaxReset();

    digitalWrite(LED_BUILTIN, LOW); // Turn LED off
}

/**
 * @brief Main loop.
 */
void loop() {
    // Read the buttons - can we move these to an interrupt?
    phone_handset.update();
    press_button.update();

    switch (mode) {
    case ERROR:
        // Error - Phone was left off hook for too long to get here
        if (phone_handset.risingEdge()) { // Handset has been replaced
            dialing_tone(OFF);
            mode = READY;
            print_mode();

            // Get the maximum number of blocks that have ever been used
            Serial.print("Ready: Max number of blocks used by Audio were: ");
            Serial.println(AudioMemoryUsageMax());
            AudioMemoryUsageMaxReset();
        } else {
            //error();
            delay(3000);

            // Get the maximum number of blocks that have ever been used
            Serial.print("Error: Max number of blocks used by Audio were: ");
            Serial.println(AudioMemoryUsageMax());
        }
        break;

    case INITIALISING:
        // Program initialising - handset not it place etc. ?????
        break;

    case READY:
        // Everything okay and ready to be used
        // Falling edge occurs when the handset is lifted --> GPO 706 telephone
        if (phone_handset.fallingEdge()) {
            Serial.println("Handset lifted");
            mode = RECORDMESSAGEPROMPT;
            print_mode();
        }

        break;

    case RECORDMESSAGEPROMPT:
        // Play message to record after the beep
        delay(1000); // Wait a second for handset to be brought up to ear
        wave_file.play("record.wav");
        while (!wave_file.isStopped()) {
            // Check if handset has been replaced
            phone_handset.update();
            if (phone_handset.risingEdge()) {
                wave_file.stop();
                Serial.println("In message prompt, set mode to ready");
                delay(500); // Time for play back to stop
                mode = READY;
                print_mode();
            }
        }

        // Check handset was not replaced above
        if (mode == RECORDMESSAGEPROMPT) {
            // Play beep to let user know to start speaking
            synth_waveform.frequency(650);
            mixer.gain(3, 1.0f);

            synth_waveform.amplitude(0.9);
            delay(750);
            synth_waveform.amplitude(0); // silence beep
            delay(250); // Delay so the message start beep is not recorded - something to look at

            start_recording();
        }
        break;

    case RECORDING:
        // Has the handset been replaced or have we exceeded the recording limit
        // ######## NEED A WARNING TONE THAT MESSAGE IS COMING TO THE MAX LENGTH ALLOWED ##############
        if (phone_handset.risingEdge()) {
            stop_recording();
            end_beep();
            mode = READY;
            print_mode();
        } else if (recording_timer >= max_recording_time) {
            Serial.print("MAX recording time exceeded: ");
            Serial.println(recording_timer);

            stop_recording();

            end_beep();

            // Play very short warning beep to indicate THE END
            // synth_waveform.frequency(700);
            // synth_waveform.amplitude(0.4);
            // delay(1000);
            // synth_waveform.amplitude(0); // silence beep

            mode = ERROR;
            print_mode();

            dialing_tone(ON);
        } else {
            // Check for coming near to end of max recording, sound a beep 'n' seconds before the end
            if (recording_timer > (max_recording_time - 10'000)) {
                sound_warning();
            }

            continue_recording();
        }
        break;

    case PLAYING:
        // Playing a recording
        break;
    }

    // Falling edge occurs when the PRESS button is pressed --> GPO 706 telephone
    if (press_button.fallingEdge()) {
        Serial.println("PRESS button pressed");
        // delay(1000);
        // start_recording();
    } else if (press_button.risingEdge()) { // PRESS button is released
        Serial.println("PRESS button released");
        delay(1000);
        if (mode != READY) {
            // stop_recording();
            // Serial.print("Max number of blocks used by Audio were: ");
            // Serial.println(AudioMemoryUsageMax());
        }
    }

    blink_led();
    //    print_time();
}

/**
 * @brief Play audio .wav file.
 */
static void play_file(const char *filename) {
    Serial.print("PLAYING file: ");
    Serial.println(filename);

    // Start playing the file.  This sketch continues to
    // run while the file plays.
    wave_file.play(filename);

    // A brief delay for the library read WAV info
    delay(25);

    // Simply wait for the file to finish playing.
    while (wave_file.isPlaying()) {
    }
}

/**
 * @brief For debugging only, print out what mode we are set to.
 */
static void print_mode(void) {
    Serial.print("Mode switched to: ");

    switch (mode) {
    case ERROR:
        Serial.println(" ERROR");
        break;

    case INITIALISING:
        Serial.println(" INITIALISING");
        break;

    case READY:
        Serial.println(" READY");
        break;

    case RECORDMESSAGEPROMPT:
        Serial.println(" RECORDMESSAGEPROMPT");
        break;

    case RECORDING:
        Serial.println(" RECORDING");
        break;

    case PLAYING:
        Serial.println(" PLAYING");
        break;

    default:
        Serial.println(" UNDEFINED");
        break;
    }
}

// WHAT DO WE DO WITH CHANGES IN BUTTONS? - TODO
/**
 * @brief Non-blocking delay, which pauses execution of main program logic, while still listening for input
 */
static void wait(unsigned int milliseconds) {
    elapsedMillis msec = 0;

    if (mode == INITIALISING) { // no need for button checking here during initialisation
        while (msec <= milliseconds) {
            // Do nothing but wait
        }
    } else {
        while (msec <= milliseconds) {
            // Currently do nothing but wait
        }

        // while (msec <= milliseconds) {
        //     phone_handset.update();
        //     press_button.update();
        //     if (phone_handset.fallingEdge()) {
        //         Serial.println("WAIT: Handset lifted");
        //     }
        //     if (press_button.fallingEdge()) {
        //         Serial.println("WAIT: PRESS button pressed");
        //     }

        //     if (phone_handset.risingEdge()) {
        //         Serial.println("WAIT Handset replaced");
        //     }
        //     if (press_button.risingEdge()) {
        //         Serial.println("WAIT: PRESS button released");
        //     }
        // }
    }
}

/**
 * @brief Play a warning to the user that recording time is coming to the end
 */
static void sound_warning(void) {
    static unsigned long previousWarningMillis;
    unsigned long timeNow = millis();

    if (timeNow - previousWarningMillis >= WARNING_DELAY) {
        previousWarningMillis = timeNow;

        // Play very short warning beep
        synth_waveform_450.frequency(450);
        synth_waveform_450.amplitude(0.3);
        delay(50);
        synth_waveform_450.amplitude(0); // silence beep
    }
}

/**
 * @brief Play a beep to indicate end of recording.
 */
static void end_beep(void) {
    synth_waveform.frequency(523.25);
    synth_waveform.amplitude(beep_volume);
    wait(250);
    synth_waveform.amplitude(0);
    wait(250);
    synth_waveform.amplitude(beep_volume);
    wait(250);
    synth_waveform.amplitude(0);
    wait(250);
    synth_waveform.amplitude(beep_volume);
    wait(250);
    synth_waveform.amplitude(0);
    wait(250);
    synth_waveform.amplitude(beep_volume);
    wait(250);
    synth_waveform.amplitude(0);
}

/**
 * @brief Play morse code.
 */
static void error(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;
    uint16_t letter_space = dot * 3;
    uint16_t word_space = dot * 7;

    // dots
    synth_waveform.amplitude(beep_volume);
    synth_waveform.frequency(800);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);

    wait(letter_space);

    // 3 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);

    wait(letter_space);

    // dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);

    wait(word_space);
}

/**
 * @brief Play morse code SD to indicate SD card error.
 */
static void sd_card_error(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;
    uint16_t letter_space = dot * 3;
    uint16_t word_space = dot * 7;

    // 3 dots
    synth_waveform.amplitude(beep_volume);
    synth_waveform.frequency(800);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);

    wait(letter_space);

    // 1 dash 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);

    wait(word_space);
}

/**
 * @brief Blink the onboard LED.
 */
static void blink_led(void) {
    static unsigned long previousMillis;
    unsigned long timeNow = millis();

    if (timeNow - previousMillis >= LED_BLINK_DELAY) {
        previousMillis = timeNow;
        if (led_state == LOW) {
            led_state = HIGH;
        } else {
            led_state = LOW;
        }

        digitalWrite(LED_BUILTIN, led_state);
    }
}

/**
 * @brief Get the time from RTC (if available).
 */
static time_t get_teensy_three_time(void) { return Teensy3Clock.get(); }

/**
 * @brief Utility function for digital clock display: prints preceding colon and leading 0.
 */
static void print_digits(int digits) {
    Serial.print(":");
    if (digits < 10)
        Serial.print('0');

    Serial.print(digits);
}

/**
 * @brief Digital clock display of the time.
 */
static void digital_clock_display(void) {
    Serial.print(hour());
    print_digits(minute());
    print_digits(second());
    Serial.print(" ");
    Serial.print(day());
    Serial.print(" ");
    Serial.print(month());
    Serial.print(" ");
    Serial.print(year());
    Serial.println();
}

/**
 * @brief Dispaly the time.
 */
static void print_time(void) {
    static unsigned long previousTimeInMillis;
    unsigned long timeNow = millis();

    if (timeNow - previousTimeInMillis >= 1000) {
        previousTimeInMillis = timeNow;

        digital_clock_display();
    }
}

// NEED TO HANDLE ERROR - SET MODE - TODO
/**
 * @brief Start recording voice to the SD card in .wav format.
 */
static void start_recording(void) {
    // Find the first available file number
    for (uint16_t i = 0; i < 9999; i++) {
        // Format the counter as a five-digit number with leading zeroes, followed by file extension
        snprintf(filename, 11, " %05d.wav", i);
        // Create if does not exist, do not open existing, write, sync after write
        if (!SD.exists(filename)) {
            break;
        }
    }

    Serial.print("start recording to file: '");
    Serial.print(filename);
    Serial.println("'");

    file_object = SD.open(filename, FILE_WRITE);
    Serial.println("Creating file.");
    if (file_object) {
        Serial.print("RECORDING to ");
        Serial.println(filename);
        queue1.begin();
        recording_timer = 0; // Reset timer to capture long recordings
        mode = RECORDING;
        print_mode();
        record_bytes_saved = 0L;
    } else {
        Serial.println("Couldn't open file to record!");
    }
}

// HAVE CHANGED AUDIO_BLOCK_SAMPLES TO 256, DO WE NEED TO CHANGE ANYTHING HERE? - TODO
/**
 * @brief Continue recording voice to the SD card.
 */
static void continue_recording(void) {
#define NBLOX 16
    // Check if there is data in the queue
    if (queue1.available() >= NBLOX) {
        byte buffer[NBLOX * AUDIO_BLOCK_SAMPLES * sizeof(int16_t)];
        // Fetch 2 blocks from the audio library and copy
        // into a 512 byte buffer.  The Arduino SD library
        // is most efficient when full 512 byte sector size
        // writes are used.
        for (int i = 0; i < NBLOX; i++) {
            memcpy(buffer + i * AUDIO_BLOCK_SAMPLES * sizeof(int16_t), queue1.readBuffer(),
                   AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
            queue1.freeBuffer();
        }
        // Write all 512 bytes to the SD card
        file_object.write(buffer, sizeof buffer);
        record_bytes_saved += sizeof buffer;
    }
}

// NEED TO HANDLE ERROR - TODO
/**
 * @brief Stop recording voice, write out any remaining data to the SD card.
 */
static void stop_recording(void) {
    Serial.println("stopRecording");
    // Stop adding any new data to the queue
    queue1.end();
    // Flush all existing remaining data from the queue
    while (queue1.available() > 0) {
        // Save to open file
        file_object.write((byte *)queue1.readBuffer(), AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
        queue1.freeBuffer();
        record_bytes_saved += AUDIO_BLOCK_SAMPLES * sizeof(int16_t);
        Serial.println("Flushed audio to file");
    }

    write_out_wav_header();

    file_object.close(); // Close the file

    Serial.println("Closed file");
    mode = READY;
    print_mode();
}

/**
 * @brief Update WAV header with final filesize/datasize variables for writing to WAV file
 */
static void write_out_wav_header(void) {
    unsigned long ChunkSize = 0L;
    unsigned long Subchunk1Size = 16;
    unsigned int AudioFormat = 1;
    unsigned int numChannels = 1;
    unsigned long sampleRate = 44100;
    unsigned int bitsPerSample = 16;
    unsigned long byteRate =
        sampleRate * numChannels * (bitsPerSample / 8); // samplerate x channels x (bitspersample / 8)
    unsigned int blockAlign = numChannels * bitsPerSample / 8;
    unsigned long Subchunk2Size = 0L;
    byte byte1, byte2, byte3, byte4;

    // NumSamples = (recByteSaved*8)/bitsPerSample/numChannels;
    // Subchunk2Size = NumSamples*numChannels*bitsPerSample/8;
    //                 number of samples x number of channels x number of bytes per sample
    Subchunk2Size =
        record_bytes_saved - 42;    // because we didn't make space for the header to start with! Lose 21 samples...
    ChunkSize = Subchunk2Size + 34; // was 36;
    file_object.seek(0);
    file_object.write("RIFF");
    byte1 = ChunkSize & 0xff;
    byte2 = (ChunkSize >> 8) & 0xff;
    byte3 = (ChunkSize >> 16) & 0xff;
    byte4 = (ChunkSize >> 24) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    file_object.write(byte3);
    file_object.write(byte4);
    file_object.write("WAVE");
    file_object.write("fmt ");
    byte1 = Subchunk1Size & 0xff;
    byte2 = (Subchunk1Size >> 8) & 0xff;
    byte3 = (Subchunk1Size >> 16) & 0xff;
    byte4 = (Subchunk1Size >> 24) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    file_object.write(byte3);
    file_object.write(byte4);
    byte1 = AudioFormat & 0xff;
    byte2 = (AudioFormat >> 8) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    byte1 = numChannels & 0xff;
    byte2 = (numChannels >> 8) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    byte1 = sampleRate & 0xff;
    byte2 = (sampleRate >> 8) & 0xff;
    byte3 = (sampleRate >> 16) & 0xff;
    byte4 = (sampleRate >> 24) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    file_object.write(byte3);
    file_object.write(byte4);
    byte1 = byteRate & 0xff;
    byte2 = (byteRate >> 8) & 0xff;
    byte3 = (byteRate >> 16) & 0xff;
    byte4 = (byteRate >> 24) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    file_object.write(byte3);
    file_object.write(byte4);
    byte1 = blockAlign & 0xff;
    byte2 = (blockAlign >> 8) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    byte1 = bitsPerSample & 0xff;
    byte2 = (bitsPerSample >> 8) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    file_object.write("data");
    byte1 = Subchunk2Size & 0xff;
    byte2 = (Subchunk2Size >> 8) & 0xff;
    byte3 = (Subchunk2Size >> 16) & 0xff;
    byte4 = (Subchunk2Size >> 24) & 0xff;
    file_object.write(byte1);
    file_object.write(byte2);
    file_object.write(byte3);
    file_object.write(byte4);
    file_object.close();
    Serial.println("header written");
    Serial.print("Subchunk2: ");
    Serial.println(Subchunk2Size);
}

/**
 * @brief Play or stop the UK dialing tone
 */
static void dialing_tone(dial_tone_state_t on_or_off) {
    if (on_or_off == ON) {
        // play 2 tones at the same time
        synth_waveform_350.amplitude(beep_volume);
        synth_waveform_350.frequency(350);
        synth_waveform_450.amplitude(beep_volume);
        synth_waveform_450.frequency(450);
    } else {
        synth_waveform_350.amplitude(0);
        synth_waveform_450.amplitude(0);
    }

    dial_tone = on_or_off;
}