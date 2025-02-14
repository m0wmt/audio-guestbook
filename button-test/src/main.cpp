#include <Arduino.h>
#include <Audio.h>
#include <Bounce.h>

#define HANDSET_PIN 41      // Handset switch
#define PRESS_PIN 40        // PRESS switch
#define LED_BLINK_DELAY 500 // Blink LED every 'n' milliseconds

// The UK dial tone combines 350 Hz and 450 Hz tones together creating a 100 Hz beat frequency.

static const uint8_t morse_time_unit = 80;      // Morse code time unit, length of a dot is 1 time unit

// Globals
AudioMixer4 mixer;                // Allows merging several inputs to same output
AudioSynthWaveform synth_waveform; // To create the "beep" sound effect
AudioSynthWaveform synth_waveform2; // To create the 2nd "beep" sound effect
AudioOutputI2S audio_output;       // I2S output to Speaker Out on Teensy 4.0 Audio shield
AudioConnection patchCord1(synth_waveform, 0, mixer, 0);
AudioConnection patchCord2(synth_waveform2, 0, mixer, 1);
AudioConnection patchCord3(mixer, 0, audio_output, 0); // mixer output to speaker (L)
AudioConnection patchCord4(mixer, 0, audio_output, 1); // mixer output to speaker (R)
AudioControlSGTL5000 audio_shield;

typedef enum { // Keep track of current state of the device
    ERROR,
    INITIALISING,
    READY,
    PROMPTING,
    WAITHANDSETTOEAR,
    WAITGREETINGSTART,
    GREETINGISPLAYING,
    RECORDING,
    PLAYING
} button_mode_t;

button_mode_t mode = INITIALISING;

float beep_volume = 0.9f; // not too loud
int led_state = LOW;      // LED state, LOW or HIGH
uint32_t wait_start = 0;

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
static void dial_tone(void);

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

    // Play a sound to indicate system is online
    sos();

    Serial.print("Max number of blocks used by Audio were: ");
    Serial.println(AudioMemoryUsageMax());

    // Is this an option?
    // phone_handset.setPressedState(LOW); // or HIGH, need to work out which if this is an option
    // phone_handset.pressed() // returns true if it was pressed
    // phone_handset.ispressed() // returs true if currently pressed
    // phone_handset.released() // returns true if if was released

    
    mode = READY;
    print_mode();

    // Reset the maximum reported by AudioMemoryUsageMax
    AudioMemoryUsageMaxReset();

    digitalWrite(LED_BUILTIN, LOW); // Turn LED off
}

void loop() {
    // Read the buttons - can we move these to an interrupt?
    phone_handset.update();
    press_button.update();

    switch (mode) {
    case ERROR:
        // Error!
        break;
    case INITIALISING:
        // Program initialising - handset not it place etc. ?????
        break;

    case READY:
        // Everything okay and ready to be used
        break;

    case PROMPTING:
        // Handset has been picked up, wait whilst it's put to the users ear
        wait_start = millis();
        mode = WAITHANDSETTOEAR;
        print_mode();
        break;

    case WAITHANDSETTOEAR:
        // Waiting a second for the user to put the handset to their ear
        if (millis() - wait_start > 1000) // give them a second
        {
            // Play the greeting message "Record message after the beep"
            // playWaveFile.play("greeting.wav");
            mode = WAITGREETINGSTART;
            print_mode();
        }
        break;

    case WAITGREETINGSTART:
        // Wait for the greeting message to start, can be a slight delay
        delay(500);
        mode = GREETINGISPLAYING;
        print_mode();
        break;

    case GREETINGISPLAYING:
        // Wait for greeting to end OR handset to is replaced
        delay(4000);
        phone_handset.update();
        if (phone_handset.risingEdge()) {
            // Handset is replaced
            Serial.println("Handset replaced during GREETINGISPLAYING");
            mode = READY;
            print_mode();
        } else {
            // Play beep to let user know it's time to speak!
            synth_waveform.frequency(600);
            synth_waveform.amplitude(0.9);
            wait(500);
            synth_waveform.amplitude(0); // silence beep
            // Start the recording function
            start_recording();
        }
        break;

    case RECORDING:
        continue_recording();
        break;

    case PLAYING:
        // Playing a recording
        break;
    }

    /* Check HANDSET and PRESS button movement and action */

    // Falling edge occurs when the handset is lifted --> GPO 706 telephone
    if (phone_handset.fallingEdge()) {
        Serial.println("Handset lifted");
        mode = PROMPTING;
        print_mode();
    } else if (phone_handset.risingEdge()) { // Handset is replaced
        Serial.println("Handset replaced");
        if (mode == RECORDING) {
            stop_recording();
            end_beep();
        } else {
            Serial.println("Not recording so reset to ready");
            mode = READY;
            print_mode();
        }
    }

    // Falling edge occurs when the PRESS button is pressed --> GPO 706 telephone
    if (press_button.fallingEdge()) {
        Serial.println("PRESS button pressed");
        // delay(1000);
        // start_recording();
    } else if (press_button.risingEdge()) { // PRESS button is released
        Serial.println("PRESS button released");
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

    while (msec <= milliseconds) {
        phone_handset.update();
        press_button.update();
        if (phone_handset.fallingEdge()) {
            Serial.println("WAIT: Handset lifted");
        }
        if (press_button.fallingEdge()) {
            Serial.println("WAIT: PRESS button pressed");
            // If pressed stop recording and wait for button press release?
            // Set mode to PREDIALING?
        }

        if (phone_handset.risingEdge()) {
            Serial.println("WAIT Handset replaced");
        }
        if (press_button.risingEdge()) {
            Serial.println("WAIT: PRESS button released");
            // When released play dialing tone and wait for dial rotations?
            // Set mode to DIALLING?
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

    case PROMPTING:
        Serial.println(" PROMPTING");
        break;

    case WAITHANDSETTOEAR:
        Serial.println(" WAITHANDSETTOEAR");
        break;

    case WAITGREETINGSTART:
        Serial.println(" WAITGREETINGSTART");
        break;

    case GREETINGISPLAYING:
        Serial.println(" GREETINGISPLAYING");
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
    mode = READY;
    print_mode();
}

/**
 * @brief Play the UK dial tone
 */
static void dial_tone(void) {
    // play 2 tones at the same time
}
