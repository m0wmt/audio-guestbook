/**
 * Audio Guestbook, based on original by Playful Technology (2022) https://github.com/playfultechnology/audio-guestbook.
 * 
 * This program is designed to record messages so a micro SD card using an old Rotary telephone at an upcoming wedding.
 * 
 * Requirements are available in the README.md file.
 */
#include <Arduino.h>


/* Variables to obtain free RAM */
extern unsigned long _heap_start;
extern unsigned long _heap_end;
extern char *__brkval;

/* Function prototypes */
static int free_ram(void);

/**
 * @brief Program setup
 */
void setup() {
  while (!Serial) {
    // Wait for serial to start!
  } 

  Serial.print("Free RAM = ");
  Serial.println(free_ram());
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
