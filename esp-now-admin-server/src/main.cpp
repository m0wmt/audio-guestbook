/** 
 * This is the admin monitor for the audio guestbook. It runs on an ESP32 and acts as a 
 * ESP-NOW server to a corresponding ESP-NOW receiver.
 * 
 * Connection between the teensy and the esp32 will be via serial, receive only for the ESP,
 * do not want anything/one messing with the guestbook. 
*/
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "config.h"

#include <HardwareSerial.h>
#include <inttypes.h>

// the setup function runs once when you press reset or power the board
#ifdef RGB_BUILTIN
#undef RGB_BUILTIN
#endif
#define RGB_BUILTIN 48

#define DEBUG true      // to turn on/off printf statements

static void send_events_via_esp_now(void);

// Teensy UART communications setup
// Define the RX pin for Serial
// Using GPIO7 for uart, default tx/rx pins interact with access point wifi for some reason!
#define RX_TEENSY 7
#define TEENSY_BAUD_RATE 9600
HardwareSerial teensy_serial(0);

typedef struct __attribute__((packed, aligned(1))) {
    uint8_t mode;
    uint16_t recordings;
    uint64_t disk_remaining;
} teensy_data_t;

teensy_data_t audio_guestbook_data;

typedef enum { // State of the audio guestbook
    ERROR,
    INITIALISING,
    READY,
    RECORDMESSAGEPROMPT,
    RECORDING,
    PLAYING,
    LEFT_OFF_HOOK
} button_mode_t;
// end of teensy information setup

// // Variables
// unsigned long last_time = 0;
// char runtime_buffer[10];

// ESP NOW
// ESP-NOW message, max size 250 bytes
typedef struct struct_message {
    uint8_t mode;
    uint16_t recordings;
    uint64_t disk_space;
    unsigned long last_time;
} struct_message;

struct_message myData;


void setup() {

    rgbLedWrite(RGB_BUILTIN, 0, 0, 0); // Turn off LED

    if (DEBUG) {
       Serial.begin(115200);

        delay(5000);  // Wait for serial to start

    
        Serial.println(F("\n##################################"));
        Serial.println(F("ESP32 Information:"));
        Serial.printf("Internal Total Heap %d, Internal Used Heap %d, Internal Free Heap %d\n", ESP.getHeapSize(),
                    ESP.getHeapSize() - ESP.getFreeHeap(), ESP.getFreeHeap());
        Serial.printf("Sketch Size %d, Free Sketch Space %d\n", ESP.getSketchSize(), ESP.getFreeSketchSpace());
        Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
        Serial.printf("Chip Model %s, ChipRevision %d, Cpu Freq %dMHz, SDK Version %s\n", ESP.getChipModel(),
                    ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
        Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
        Serial.println(F("##################################\n\n"));
    }
 
    if (DEBUG) {
        Serial.println("Setting Up ESP-NOW");
    }

    WiFi.mode(WIFI_STA);
    while (WiFi.status()==WL_STOPPED){}

    if (esp_now_init() != ESP_OK) {
        if (DEBUG) {
            Serial.println("ESP-NOW initialization failed");
        }
        return;
    }

    if (DEBUG) {
        Serial.println("=== ESP32 MAC Address ===");
        Serial.print("STA MAC:  ");
        Serial.println(WiFi.macAddress());
    }

    // Set PMK key
    esp_now_set_pmk((uint8_t *)PMK);
    
    // Register peer device
    esp_now_peer_info_t peerInfo = {};
    memset(&peerInfo, 0, sizeof(peerInfo)); // Clear junk data

    memcpy(peerInfo.peer_addr, receiverMac, 6);
    peerInfo.channel = 0;  // 0 means use current channel
    peerInfo.encrypt = true;
    //Set the receiver device LMK key
    for (uint8_t i = 0; i < 16; i++) {
        peerInfo.lmk[i] = LMK[i];
    }

    // Add receiver as peer        
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        if (DEBUG) {
            Serial.println("Failed to add peer (AMOLED display)");
        }
        return;
    }
    
    if (DEBUG) {
        Serial.println("ESP-NOW master ready");    
    }

    // Reset data
    myData.mode = INITIALISING;
    myData.recordings = 0;
    myData.disk_space = 0;
    myData.last_time = 0;

    // Set up UART communications (UART0) to Teensy. Rx only will be used,
    // there will be no transmit to the Teensy
    teensy_serial.begin(TEENSY_BAUD_RATE, SERIAL_8N1, RX_TEENSY);

    audio_guestbook_data.disk_remaining = 0;
    audio_guestbook_data.recordings = 0;
    audio_guestbook_data.mode = INITIALISING;
}

void loop() {
    if (teensy_serial.available() > 0) {
        // // get the byte of data from the Teensy
        byte n = teensy_serial.available();
        {
            if (n != 0) {
                if (n >= 4) {
                    // Serial.println(n);//debugg
                    uint32_t syncPatt = (uint32_t)teensy_serial.read() << 24 | (uint32_t)teensy_serial.read() << 16 |
                                        (uint32_t)teensy_serial.read() << 8 | teensy_serial.read();
                    if (DEBUG) {
                        Serial.print("Sync: "); Serial.println(syncPatt, HEX);//debug
                    }
                    delay(10); // without delay, the Receiver does not work! Why?
                    if (syncPatt == 0xDEADBEEF) {
                        byte p = teensy_serial.read(); // number of bytes in struct
                        byte m = teensy_serial.readBytes((byte *)&audio_guestbook_data, p);
                        
                        // Debug only printing
                        if (DEBUG) {
                            Serial.print("Mode = ");
                            switch (audio_guestbook_data.mode) {
                                case ERROR:
                                    Serial.print("ERROR");
                                    break;

                                case INITIALISING:
                                    Serial.print("INITIALISING");
                                    break;

                                case READY:
                                    Serial.print("READY");
                                    break;

                                case RECORDMESSAGEPROMPT:
                                    Serial.print("RECORDMESSAGEPROMPT");
                                    break;

                                case RECORDING:
                                    Serial.print("RECORDING");
                                    break;

                                case PLAYING:
                                    Serial.print("PLAYING");
                                    break;

                                case LEFT_OFF_HOOK:
                                    Serial.print("LEFT_OFF_HOOK");
                                    break;

                                default:
                                    Serial.print("UNDEFINED");
                                    break;
                            }

                            Serial.print("   ");
                            Serial.print("Recordings = ");
                            Serial.print(audio_guestbook_data.recordings);
                            Serial.print("   ");
                            Serial.print("Disk Remaining = ");
                            Serial.print(audio_guestbook_data.disk_remaining);
                            Serial.println(' ');
                            Serial.println("===========================");
                        }

                        send_events_via_esp_now();
                    }
                }
            }
        }
    }
}

/**
 * @brief Send events to the ESP NOW receiver
 */
static void send_events_via_esp_now(void) {
  
    // // So the user knows the application is still running!
    // last_time = millis();

    // sprintf(runtime_buffer, "%02d:%02d:%02d", (last_time / 1000) / 3600, ((last_time / 1000) % 3600) / 60,
    //         ((last_time / 1000) % 3600) % 60);

    // sprintf(myData.runtime, "%02d:%02d:%02d", (last_time / 1000) / 3600, ((last_time / 1000) % 3600) / 60,
    //         ((last_time / 1000) % 3600) % 60);

    myData.last_time = millis();
    myData.recordings = audio_guestbook_data.recordings;
    myData.mode = audio_guestbook_data.mode;
    myData.disk_space = audio_guestbook_data.disk_remaining;

    esp_err_t result = esp_now_send(receiverMac, (uint8_t *)&myData, sizeof(myData));
    
    if (DEBUG) {
        if (result == ESP_OK) {
            Serial.printf("Send: \n  recordings=%u, disk space=%llu, mode %d, runtime %lu\n", myData.recordings, myData.disk_space, myData.mode, myData.last_time);
        } else {
            Serial.println("Send failed");
        }    
    }
}