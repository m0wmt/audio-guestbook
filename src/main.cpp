/**
 * Audio Guestbook, based on original by Playful Technology (2022) https://github.com/playfultechnology/audio-guestbook.
 * 
 * This program is designed to record messages so a micro SD card using an old Rotary telephone at an upcoming wedding.
 * 
 * Requirements are available in the README.md file.
 */
#include <Arduino.h>
#include <Wire.h>

/* Headers to map pins to ports */
#include <algorithm>
#include "pin.h"

/* Variables to obtain free RAM */
extern unsigned long _heap_start;
extern unsigned long _heap_end;
extern char *__brkval;

/* Function prototypes */
static int free_ram(void);
static void map_pins_to_ports(void);

/**
 * @brief Program setup
 */
void setup() {
  while (!Serial) {
    // Wait for serial to start!
  } 

  Serial.print("Free RAM = ");
  Serial.println(free_ram());

  map_pins_to_ports();
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
 * @brief Display GPIO registers to the digital pins and vice versa.
 */
static void map_pins_to_ports(void) {
    Pin *pins1[CORE_NUM_DIGITAL];
    Pin *pins2[CORE_NUM_DIGITAL];

    for (unsigned pinNr = 0; pinNr < CORE_NUM_DIGITAL; pinNr++)
    {
        Pin *p = new Pin(pinNr);
        pins1[pinNr] = p;
        pins2[pinNr] = p;
    }

    std::sort(pins1, pins1 + CORE_NUM_DIGITAL, [](Pin *a, Pin *b)  // Sort by pin number
    {
        return a->getPinNr() < b->getPinNr(); 
    }); 

    std::sort(pins2, pins2 + CORE_NUM_DIGITAL, [](Pin *a, Pin *b)   // Sort by GPIO and Bit
    {
        if (a->getGpioNr() < b->getGpioNr()) return true;
        if (a->getGpioNr() > b->getGpioNr()) return false;
        if (a->getBitNr() < b->getBitNr())   return true;
        return false;
    });

    // Print results in two columns--------------------------
    Serial.println("PIN   GPIOn-BITm  |  GPIOn-BITm    PIN");
    Serial.println("------------------|-------------------");
    for (unsigned i = 0; i < CORE_NUM_DIGITAL; i++)
    {
        unsigned pin1 = pins1[i]->getPinNr();
        unsigned pin2 = pins2[i]->getPinNr();
        unsigned gpio1 = pins1[i]->getGpioNr();
        unsigned gpio2 = pins2[i]->getGpioNr();
        unsigned bit1 = pins1[i]->getBitNr();
        unsigned bit2 = pins2[i]->getBitNr();
        Serial.printf("%02d  -> GPIO%u-%02u   |   GIPO%u-%02u  ->  %02d\n", pin1, gpio1, bit1, gpio2, bit2, pin2);
    }    
}