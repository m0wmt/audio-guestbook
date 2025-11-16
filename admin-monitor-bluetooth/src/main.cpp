
/*
 *
 * This sketch emulates parts of a Polar H7 Heart Rate Sensor.
 * It exposes the Heart rate and the Sensor position characteristics

   Copyright <2017> <Andreas Spiess>

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Based on Neil Kolban's example file: https://github.com/nkolban/ESP32_BLE_Arduino
 */
/*
https://circuitdigest.com/microcontroller-projects/esp32-ble-server-how-to-use-gatt-services-for-battery-level-indication

https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf
service by UUID
0x1801 - GATT service
0x183F - elapsed time service
0x180A - device information service
0x181C - user data service
0x183B - binary sensor service
0x184C - generic telephone bearer service
0x184E - audio stream control service
0x1853 - common audio service
0x1855 - telephony and media audio service
0x1856 - public broadcast announcement service
0x185A - industrial measurement device service

units by name
time (day) 0x2762
time (hour) 0x2761
time (minute) 0x2760
time (month) 0x27B4
time (second) 0x2703
time (year) 0x27B3
parts per billion 0x27C5
parts per million 0x27C4
unitless 0x2700

charaistic by name
alert status - 0x2A3F
Audio Input Control Point 0x2B7B
Audio Input Description 0x2B7C
Audio Input State 0x2B77
Audio Input Status 0x2B7A
Audio Input Type 0x2B79
Audio Location 0x2B81
Audio Output Description 0x2B83
Device Name 0x2A00
Object Size 0x2AC0
Current Track Object ID 0x2B9D
Track Changed 0x2B96
Track Duration 0x2B98
Track Position 0x2B99
Track Title 0x2B97

Context type
0x0004 Media

*/
#include <Arduino.h>

#include <HardwareSerial.h>
#include <inttypes.h>

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

char bluetooth_buffer[120];

bool _BLEClientConnected = false;
#define DeviceInformationService BLEUUID((uint16_t)0x181C)

// BLECharacteristic SizeCharacteristic(BLEUUID((uint16_t)0x2A8E), BLECharacteristic::PROPERTY_READ |
//                                                                                  BLECharacteristic::PROPERTY_NOTIFY);
// BLEDescriptor SizeDescriptor(BLEUUID((uint16_t)0x2A8E));

// BLECharacteristic RecordingCharacteristic(BLEUUID((uint16_t)0x2B9D),
//                                           BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
// BLEDescriptor RecordingDescriptor(BLEUUID((uint16_t)0x2B9D));

// BLECharacteristic StatusCharacteristic(BLEUUID((uint16_t)0x2B2E),
//                                        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
// BLEDescriptor StatusDescriptor(BLEUUID((uint16_t)0x2B2E));

BLECharacteristic infoCharacteristic(BLEUUID((uint16_t)0x2AF4),
                                     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor infoDescriptor(BLEUUID((uint16_t)0x2AF4));

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) { _BLEClientConnected = true; };
    void onDisconnect(BLEServer *pServer) { _BLEClientConnected = false; }
};
void InitBLE() {
    BLEDevice::init("BLE Guestbook");
    // Create the BLE Server
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    // Create the BLE Service
    BLEService *pService = pServer->createService(DeviceInformationService);

    // Set up run time buffer
    // sprintf(runtime_buffer, "%02d:%02d:%02d", 0, 0, 5);

    // pService->addCharacteristic(&SizeCharacteristic);
    // SizeDescriptor.setValue("Size");
    // SizeCharacteristic.addDescriptor(&SizeDescriptor);
    // //SizeCharacteristic.addDescriptor(new BLE2902());

    // pService->addCharacteristic(&RecordingCharacteristic);
    // RecordingDescriptor.setValue("Recording");
    // RecordingCharacteristic.addDescriptor(&RecordingDescriptor);
    // //RecordingCharacteristic.addDescriptor(new BLE2902());

    // pService->addCharacteristic(&StatusCharacteristic);
    // StatusDescriptor.setValue("Status");
    // StatusCharacteristic.addDescriptor(&StatusDescriptor);
    // //StatusCharacteristic.addDescriptor(new BLE2902());

    pService->addCharacteristic(&infoCharacteristic);
    infoDescriptor.setValue("Guestbook Status");
    infoCharacteristic.addDescriptor(&infoDescriptor);
//    infoCharacteristic.addDescriptor(new BLE2902());

    pServer->getAdvertising()->addServiceUUID(DeviceInformationService);
    pServer->getAdvertising()->setScanResponse(true);
    pServer->getAdvertising()->setMinPreferred(0x06);   // to help with iPhone connection issues
    pServer->getAdvertising()->setMinPreferred(0x12);
    pService->start();
    // Start advertising
    pServer->getAdvertising()->start();
}
void setup() {
    Serial.begin(115200);
    InitBLE();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

uint8_t level = 57;
uint8_t track = 1;
void loop() {
    sprintf(bluetooth_buffer, "STATUS: Recording; Recordings %d; Disk Space %d Bytes; Runtime 01:02:%02d", level, track, track);
    infoCharacteristic.setValue(bluetooth_buffer);
    infoCharacteristic.notify();

    delay(5000);
    level++;
    track++;
    Serial.println(int(level));
    if (int(level) == 100)
        level = 0;
    if (int(track) == 59)
        track = 0;
}
