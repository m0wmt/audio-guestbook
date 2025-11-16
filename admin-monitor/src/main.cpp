/** 
 * This is the admin monitor for the audio guestbook. It runs on an ESP32 and has a wi-fi
 * access point (password protected) so admin user can monitor how the guestbook is doing. 
 * 
 * Connection between the teensy and the esp32 will be via serial, receive only for the ESP,
 * do not want anything/one messing with the guestbook. 
*/
#include <Arduino.h>

// Load Wi-Fi library
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "../include/index.h"
#include "../include/config.h"

#include <HardwareSerial.h>
#include <inttypes.h>

static String processor(const String &var);
static void send_events_to_web_client(void);

// Teensy UART communications setup
// Define the RX and TX pins for Serial 2
#define RX_TEENSY 16
#define TX_TEENSY 17 // Will not be used but set up
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
    PLAYING
} button_mode_t;
// end of teensy information setup

// Set web server port number to 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Variables
unsigned long last_time = 0;
char runtime_buffer[10];


void setup() {

    // TODO: Turn off all LEDs to save power

    Serial.begin(115200);

    delay(5000);

    Serial.println(F("\n##################################"));
    Serial.println(F("ESP32 Information:"));
    Serial.printf("Internal Total Heap %d, Internal Used Heap %d, Internal Free Heap %d\n", ESP.getHeapSize(),
                  ESP.getHeapSize() - ESP.getFreeHeap(), ESP.getFreeHeap());
    Serial.printf("Sketch Size %d, Free Sketch Space %d\n", ESP.getSketchSize(), ESP.getFreeSketchSpace());
    Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    Serial.printf("Chip Model %s, ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipModel(),
                  ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
    Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
    Serial.println(F("##################################\n\n"));

    // Change CPU frequency to save power!

    Serial.println(F("\n##################################"));
    Serial.printf("Changing CPU frequency to 80MHz to save power\n");
    setCpuFrequencyMhz(80);
    Serial.printf("Cpu Freq Now %dMHz\n", ESP.getCpuFreqMHz());
    Serial.println(F("##################################\n\n"));
    delay(1000);    // Just to give time for everything to settle down

    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)…");

    // Taken from config.h
    WiFi.softAP(SSID, WIFI_PASSWORD);

    Serial.print("Connecting to: ");
    Serial.println(SSID);

    // If connection successful show IP address in serial monitor
    Serial.print("Connected to: ");
    Serial.println(SSID);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Handle Web Server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", MAIN_page, processor); });

    // Handle Web Server Events
    events.onConnect([](AsyncEventSourceClient *client) {
        if (client->lastId()) {
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        // send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!", NULL, millis(), 10000);
    });
    server.addHandler(&events);
    server.begin(); // Start server

    Serial.println("HTTP server started");
    Serial.println("");

    // Set up run time buffer to 5 seconds, waiting time above!
    sprintf(runtime_buffer, "%02d:%02d:%02d", 0, 0, 5);

    // Set up UART communications (UART0) to Teensy. Rx only will be used,
    // there will be no transmit to the Teensy
    teensy_serial.begin(TEENSY_BAUD_RATE, SERIAL_8N1);

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
                    Serial.print("Sync: "); Serial.println(syncPatt, HEX);//debug
                    delay(10); // without delay, the Receiver does not work! Why?
                    if (syncPatt == 0xDEADBEEF) {
                        byte p = teensy_serial.read(); // number of bytes in struct
                        byte m = teensy_serial.readBytes((byte *)&audio_guestbook_data, p);
                        
                        // Debug only printing
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

                        send_events_to_web_client();
                    }
                }
            }
        }
    }
}

/**
 * @brief Handle requests from the web page.
 */
static String processor(const String &var) {
    Serial.println(var);
    
    if (var == "DISKSPACE") {
        return String(audio_guestbook_data.disk_remaining);
    } else if (var == "STATUS") {
        if (audio_guestbook_data.mode == READY) {
            return "READY";
        } else if (audio_guestbook_data.mode == RECORDING || audio_guestbook_data.mode == RECORDMESSAGEPROMPT) {
            return "RECORDING";
        } else if (audio_guestbook_data.mode == PLAYING) {
            return "PLAYING";
        } else if (audio_guestbook_data.mode == INITIALISING) {
            return "INITIALISING";
        } else {
            return "ERROR";
        }
    } else if (var == "RECORDINGS") {
        return String(audio_guestbook_data.recordings);
    } else if (var == "RUNTIME") {
        return String(runtime_buffer);
    }

    return String();
}

/**
 * @brief Send events to the web client so they can be viewed on the web page.
 */
static void send_events_to_web_client(void) {
    events.send("ping", NULL, millis());
    events.send(String(audio_guestbook_data.disk_remaining).c_str(), "diskspace", millis());

    if (audio_guestbook_data.mode == READY) {
        events.send("READY", "status", millis());
    } else if (audio_guestbook_data.mode == RECORDING || audio_guestbook_data.mode == RECORDMESSAGEPROMPT) {
        events.send("RECORDING", "status", millis());
    } else if (audio_guestbook_data.mode == PLAYING) {
        events.send("PLAYING", "status", millis());
    } else if (audio_guestbook_data.mode == INITIALISING) {
        events.send("INITIALISING", "status", millis());
    } else {
        events.send("ERROR", "status", millis());
    }

    events.send(String(audio_guestbook_data.recordings).c_str(), "recordings", millis());

    // So the user knows the application is still running!
    last_time = millis();

    sprintf(runtime_buffer, "%02d:%02d:%02d", (last_time / 1000) / 3600, ((last_time / 1000) % 3600) / 60,
            ((last_time / 1000) % 3600) % 60);

    Serial.print("Run time: ");
    Serial.println(runtime_buffer);
    events.send(String(runtime_buffer), "runtime", millis());
}