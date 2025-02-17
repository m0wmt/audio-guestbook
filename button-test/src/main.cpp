/**
 * Audio Guestbook
 * 
 * Let guests leave a message so the bride and groom can relive the day and hear what their loved
 * ones/friends had to say via an old BT rotary telephone (using a Teensy development board and a 
 * Teensy Audio board) to record messages to a micro SD card.
 *
 *
 *
 * The UK dial tone combines 350 Hz and 450 Hz tones together creating a 100 Hz beat frequency.
 */
#include <Arduino.h>
#include <Audio.h>
#include <Bounce.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <TimeLib.h>
#include <Wire.h>

//#include "play_sd_wav.h"

#define HANDSET_PIN 41      // Handset switch
#define PRESS_PIN 40        // PRESS switch
#define LED_BLINK_DELAY 500 // Blink LED every 'n' milliseconds

// Use Teensy 4.1 SD card instead of the audio boards SD card interface.
#define SDCARD_CS_PIN BUILTIN_SDCARD
#define SDCARD_MOSI_PIN 11
#define SDCARD_SCK_PIN 13

static const uint8_t morse_time_unit = 80; // Morse code time unit, length of a dot is 1 time unit in milliseconds
static const uint32_t max_recording_time = 10'000; // Recording time limit (milliseconds)
// 60000 = 1 min
// 120000 =  2 mins
// 240000 = 4 mins


// Globals
AudioMixer4 mixer;                     // Allows merging several inputs to same output
AudioPlaySdWav wave_file;              // Play 44.1kHz 16-bit PCM .WAV files
AudioSynthWaveform synth_waveform;     // To create the "beep" sound effect
AudioSynthWaveform synth_waveform_350; // To create UK dial tone
AudioSynthWaveform synth_waveform_450; // To create UK dial tone
AudioOutputI2S audio_output;           // I2S output to Speaker Out on Teensy 4.0 Audio shield
AudioConnection connection_one(wave_file, 0, mixer, 0);
AudioConnection connection_two(synth_waveform, 0, mixer, 1);
AudioConnection connection_three(synth_waveform_350, 0, mixer, 2);
AudioConnection connection_four(synth_waveform_450, 0, mixer, 3);
AudioConnection connection_five(mixer, 0, audio_output, 0); // mixer output to speaker (L)
AudioConnection connection_six(mixer, 0, audio_output, 1);  // mixer output to speaker (R)
AudioControlSGTL5000 audio_shield;

typedef enum { // Keep track of current state of the device
    ERROR,
    INITIALISING,
    READY,
    RECORDMESSAGEPROMPT,
    //    RECORDMESSAGEISPLAYING,
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
uint32_t wait_start = 0;
elapsedMillis recording_timer = 0; // Recording timer to prevent long messages

// Debounce on switches
Bounce phone_handset = Bounce(HANDSET_PIN, 40);
Bounce press_button = Bounce(PRESS_PIN, 40);

static void sos(void);
static void blink_led(void);
static void end_beep(void);
static void start_recording(void);
static void continue_recording(void);
static void stop_recording(void);
static void print_mode(void); // for debugging only
static void wait(unsigned int milliseconds);
static void dialing_tone(dial_tone_state_t on_or_off);

static void a(void);
static void b(void);
static void c(void);
static void d(void);
static void e(void);
static void f(void);
static void g(void);
static void h(void);
static void i(void);
static void j(void);
static void k(void);
static void l(void);
static void m(void);
static void n(void);
static void o(void);
static void p(void);
static void q(void);
static void r(void);
static void s(void);
static void t(void);
static void u(void);
static void v(void);
static void w(void);
static void x(void);
static void y(void);
static void z(void);
static void letter_space(void);
static void word_space(void);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);    // Orange LED on board
    digitalWrite(LED_BUILTIN, HIGH); // Glowing whilst running through setup code

    delay(1000); // Delay so serial has time to start

    Serial.printf("Audio block set to %d samples\n", AUDIO_BLOCK_SAMPLES);

    print_mode();

    // Configure the input pins
    pinMode(HANDSET_PIN, INPUT_PULLUP);
    pinMode(PRESS_PIN, INPUT_PULLUP);

    // Reset the maximum reported by AudioMemoryUsageMax
    AudioMemoryUsageMaxReset();

    // Audio connections require memory to work. The numberBlocks input
    // specifies how much memory to reserve for audio data. Each block holds 256
    // audio samples, or approx 5.8 ms of sound.
    AudioMemory(10);

    // Comment these out if not using the audio adaptor board.
    audio_shield.enable();
    audio_shield.volume(0.6);

    mixer.gain(0, 1.0f);
    mixer.gain(1, 1.0f);

    // sos();
    dialing_tone(ON);

    // Init SD CARD
    SPI.setMOSI(SDCARD_MOSI_PIN);
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(SDCARD_CS_PIN))) {
        // Stop here, and print a message repetitively, this will cause the LED to stay lit (not that anyone can see
        // this!) and the beep tone to carry on playing to indicate an error.
        while (1) {
            Serial.println("Unable to access the SD card");
            delay(5000);
        }
    } else {
        Serial.println("SD card present");
    }

    delay(3000);

    // Check handset is on phone, if not wait until it is.

    // Is this an option? Not without bounce2
    // phone_handset.setPressedState(LOW); // or HIGH, need to work out which if this is an option
    // phone_handset.pressed() // returns true if it was pressed
    // phone_handset.ispressed() // returs true if currently pressed
    // phone_handset.released() // returns true if if was released

    // phone_handset.update();
    // uint8_t read = phone_handset.read();
    // while (read != 1) {
    //     // wait for handset to be replaced
    //     dial_tone();
    //     phone_handset.update();
    //     read = phone_handset.read();
    // }

    mode = READY;
    print_mode();

    // o();
    // letter_space();
    // k();
    // letter_space();

    dialing_tone(OFF);

    Serial.print("Max number of blocks used by Audio were: ");
    Serial.println(AudioMemoryUsageMax());

    AudioMemoryUsageMaxReset(); // Reset the maximum reported by AudioMemoryUsageMax

    digitalWrite(LED_BUILTIN, LOW); // Turn LED off
}

void loop() {
    // Read the buttons
    phone_handset.update();
    press_button.update();

    switch (mode) {
    case ERROR:
        // Error - Phone was left off hook for too long to get here
        if (phone_handset.risingEdge()) { // Handset has been replaced
            dialing_tone(OFF);
            mode = READY;
            print_mode();
        } else {
            end_beep();
            delay(3000);
        }
        break;

    case INITIALISING:
        // To keep compiler happy as it should never get here
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
        Serial.println("Play record message .wav file...");
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
            Serial.println("Record message .wav file ended");
            // Play beep to let user know to start speaking
            synth_waveform.frequency(650);
            synth_waveform.amplitude(0.9);
            delay(750);
            synth_waveform.amplitude(0); // silence beep

            start_recording();
        }
        break;

        // case RECORDMESSAGEISPLAYING:
        //     // Wait for greeting to end OR handset to is replaced
        //     delay(4000); // delay here for debug purposes
        //     phone_handset.update();
        //     if (phone_handset.risingEdge()) {
        //         // Handset is replaced
        //         Serial.println("Handset replaced during RECORDMESSAGEISPLAYING");
        //         mode = READY;
        //         print_mode();
        //     } else {
        //         // Play beep to let user know it's time to speak!
        //         synth_waveform.frequency(650);
        //         synth_waveform.amplitude(0.9);
        //         wait(500);
        //         synth_waveform.amplitude(0); // silence beep
        //         // Start the recording function
        //         start_recording();
        //     }
        //     break;

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
            delay(1000);

            sos();

            //            dialing_tone(ON);
            //            delay(1000);

            mode = ERROR;
            print_mode();
        } else {
            continue_recording();
        }
        break;

    case PLAYING:
        // Playing a recording
        break;
    }

    /* Check HANDSET and PRESS button movement and action */

    // Falling edge occurs when the handset is lifted --> GPO 706 telephone

    // MOVED TO SWITCH STATEMENT
    // if (phone_handset.fallingEdge()) {
    //     Serial.println("Handset lifted");
    //     mode = RECORDMESSAGEPROMPT;
    //     print_mode();
    // } else if (phone_handset.risingEdge()) { // Handset is replaced
    //     Serial.println("Handset replaced");
    //     Serial.print("Current mode is: ");
    //     print_mode();

    //     if (mode == RECORDING) {
    //         stop_recording();
    //         end_beep();
    //         // CHECK OTHER MODES AND STOP PLAYING WAV FiLE if PLAYiNG AND THEN SET To READY
    //         // OR IF WE ARE IN DIALING MODE STOP WHAT WE WERE DOING WHICH COULD HAVE BEEN
    //         // PLAYING A WAV FILE.
    //     } else {
    //         Serial.println("Not recording so reset to ready");
    //         mode = READY;
    //         print_mode();
    //     }
    // }

    // Falling edge occurs when the PRESS button is pressed --> GPO 706 telephone
    if (press_button.fallingEdge()) {
        Serial.println("PRESS button pressed");
        // delay(1000);
        // stop_recording();
    } else if (press_button.risingEdge()) { // PRESS button is released
        Serial.println("PRESS button released");
        // PLAY DIALING TONE READY FOR PHONE NUMBER! NEED A NEW MODE HERE
        // delay(1000);
        // if (mode != READY) {
        //     stop_recording();
        // }
    }

    blink_led();
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
            // phone_handset.update();
            // press_button.update();
            // if (phone_handset.fallingEdge()) {
            //     Serial.println("WAIT: Handset lifted");
            // }
            // if (press_button.fallingEdge()) {
            //     Serial.println("WAIT: PRESS button pressed");
            //     // If pressed stop recording and wait for button press release?
            //     // Set mode to PREDIALING?
            // }

            // if (phone_handset.risingEdge()) {
            //     Serial.println("WAIT Handset replaced");
            //     // See what mode we are and react accordingly
            //     if (mode == RECORDMESSAGEPROMPT) {
            //         // not playing message prompt yet, revert to ready mode
            //         mode = READY;
            //         print_mode();
            //     } else if (mode == RECORDING) {
            //         stop_recording();
            //     }
            // }
            // if (press_button.risingEdge()) {
            //     Serial.println("WAIT: PRESS button released");
            //     // When released play dialing tone and wait for dial rotations?
            //     // Set mode to DIALLING?
            // }
        }
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

        // case PROMPTING:
        //     Serial.println(" PROMPTING");
        //     break;

        // case WAITHANDSETTOEAR:
        //     Serial.println(" WAITHANDSETTOEAR");
        //     break;

        // case WAITGREETINGSTART:
        //     Serial.println(" WAITGREETINGSTART");
        //     break;

        // case RECORDMESSAGEISPLAYING:
        //     Serial.println(" RECORDMESSAGEISPLAYING");
        //     break;

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
 * @brief Play morse code SOS.
 */
static void sos(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;
    uint16_t letter_space = dot * 3;
    uint16_t word_space = dot * 7;

    // 3 dots
    wait(symbol_space);
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

// NEED TO HANDLE ERROR - SET MODE - TODO
/**
 * @brief Start recording voice to the SD card in .wav format.
 */
static void start_recording(void) {
    mode = RECORDING;
    print_mode();

    recording_timer = 0; // Reset timer to capture long recordings
    // NEED TO ADD AN ERROR HERE FOR TESTING
}

/**
 * @brief Continue recording voice to the SD card.
 */
static void continue_recording(void) {
    // Keep for logic testing only
}

// NEED TO HANDLE ERROR - TODO
/**
 * @brief Stop recording voice, write out any remaining data to the SD card.
 */
static void stop_recording(void) {
    Serial.println("Stopped Recording");
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

/**
 * @brief Play morse code letter a (.-)
 */
static void a(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    synth_waveform.frequency(800);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter b (-...)
 */
static void b(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 3 dots
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
}

/**
 * @brief Play morse code letter c (-.-.)
 */
static void c(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter d (-..)
 */
static void d(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter e (.)
 */
static void e(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter f (..-.)
 */
static void f(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter g (--.)
 */
static void g(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 2 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter h (....)
 */
static void h(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 4 dots
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
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter i (..)
 */
static void i(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter j (.---)
 */
static void j(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

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
    wait(symbol_space);
}

/**
 * @brief Play morse code letter k (-.-)
 */
static void k(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter l (.-..)
 */
static void l(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter m (--)
 */
static void m(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 2 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter n (-.)
 */
static void n(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter o (---)
 */
static void o(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

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
    wait(symbol_space);
}

/**
 * @brief Play morse code letter p (.--.)
 */
static void p(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 2 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter q (--.-)
 */
static void q(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 2 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter r (.-.)
 */
static void r(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter s (...)
 */
static void s(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 3 dots
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
    wait(symbol_space);
}

/**
 * @brief Play morse code letter t (-)
 */
static void t(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter u (..-)
 */
static void u(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter v (...-)
 */
static void v(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 3 dots
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
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter w (.--)
 */
static void w(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    Serial.println("Morse w");
    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 2 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter x (-..-)
 */
static void x(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter y (-.--)
 */
static void y(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 1 dash
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 1 dot
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 2 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Play morse code letter z (--..)
 */
static void z(void) {
    // Morse code timings
    uint8_t dot = morse_time_unit; // milliseconds
    uint16_t dash = dot * 3;
    uint8_t symbol_space = dot;

    // 2 dashes
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dash);
    synth_waveform.amplitude(0);
    wait(symbol_space);

    // 2 dots
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
    synth_waveform.amplitude(beep_volume);
    wait(dot);
    synth_waveform.amplitude(0);
    wait(symbol_space);
}

/**
 * @brief Morse code letter space wait
 */
static void letter_space(void) {
    // Morse code timings
    uint16_t letter_space = morse_time_unit * 3;

    wait(letter_space);
}

/**
 * @brief Morse code word space wait
 */
static void word_space(void) {
    // Morse code timings
    uint16_t word_space = morse_time_unit * 7;

    wait(word_space);
}
