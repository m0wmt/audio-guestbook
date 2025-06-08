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
//#include <WebServer.h>
#include <WiFi.h>

#include "../include/index.h"

static String processor(const String &var);
    
// Replace with your network credentials
const char *ssid = "Guestbook-AP";
const char *password = "123456789"; // temp password!

// Set web server port number to 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Timer variables
unsigned long last_time = 0;
unsigned long timer_delay = 30000;
float disk_space = 14.0;
unsigned int recordings = 7;
unsigned int status = 1;

void setup() {

    // TODO: Turn off all LEDs to save power

    Serial.begin(115200);

    delay(3000);

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

    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)…");

    // Remove the password parameter, if you want the AP (Access Point) to be open
    WiFi.softAP(ssid, password);

    Serial.println("Connecting to ");
    Serial.print(ssid);

    // If connection successful show IP address in serial monitor
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);

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
}

void loop() {
    if ((millis() - last_time) > timer_delay) {
        disk_space -= 0.1;
        recordings += 1;
        Serial.printf("Disk Space = %.2f Gb", disk_space);
        Serial.println();
        Serial.printf("Recordings= %d", recordings);
        Serial.println();
        Serial.printf("Status = %d", status);
        Serial.println();

        // Send Events to the Web Client with the Sensor Readings
        events.send("ping", NULL, millis());
        events.send(String(disk_space).c_str(), "diskspace", millis());

        if (status == 1)
            events.send("OK", "status", millis());
        else
            events.send("ERROR", "status", millis());

        events.send(String(recordings).c_str(), "recordings", millis());

        last_time = millis();
    }
}

/**
 * @brief Handle requests from the web page
 */
static String processor(const String &var) {
    Serial.println(var);
    
    if (var == "DISKSPACE") {
        return String(disk_space);
    } else if (var == "STATUS") {
        if (status == 1)
            return "OK";
        else
            return "ERROR";
    } else if (var == "RECORDINGS") {
        return String(recordings);
    }
    return String();
}