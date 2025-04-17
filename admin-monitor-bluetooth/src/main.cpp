#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define SERVICE_UUID "ba1faa6e-1c9f-4475-9bbf-b84815066363"
#define CHARACTERISTIC_UUID "ba1faa6e-1c9f-4475-9bbf-b84815066364"
#define RECORDINGS_UUID "ba1faa6e-1c9f-4475-9bbf-b84815066365"

#define RED_LED 48

uint64_t disk_space = 1400000000;
unsigned int recordings = 0;
bool deviceConnected = false;
bool oldDeviceConnected = true;

BLECharacteristic *pCharacteristic = NULL;
BLECharacteristic *recordingsCharacteristic = NULL;
BLEServer *pServer = NULL;
BLEService *pService = NULL;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) { deviceConnected = true; };

    void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};

void setup() {
    // pinMode(48, OUTPUT);
    //     pinMode(33, OUTPUT);

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

    Serial.println("Starting BLE work!");

    BLEDevice::init("Audio Guestbook");
    pServer = BLEDevice::createServer();
    pService = pServer->createService(SERVICE_UUID);

    pCharacteristic =
        pService->createCharacteristic(CHARACTERISTIC_UUID,
                                       BLECharacteristic::PROPERTY_READ | // BLECharacteristic::PROPERTY_WRITE |
                                           BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

    pCharacteristic->setValue("Disk Space");

    recordingsCharacteristic = pService->createCharacteristic(
        RECORDINGS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
    recordingsCharacteristic->setValue("Recordings");

    pService->start();

    // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("Characteristic defined! Now you can read it in your phone!");

    // digitalWrite(RED_LED, LOW);
}

void loop() {
    // put your main code here, to run repeatedly:
    // notify changed value
    // if (deviceConnected) {
    pCharacteristic->setValue(String(disk_space).c_str());
    pCharacteristic->notify();
    recordingsCharacteristic->setValue(String(recordings).c_str());
    recordingsCharacteristic->notify();

    Serial.print("New values notified: ");
    Serial.print(disk_space); 
    Serial.print(", "); 
    Serial.println(recordings);
    
    recordings += 1;
    disk_space -= 5;

    delay(5000); // bluetooth stack will go into congestion if too many packets are sent
    //}

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        Serial.println("Device disconnected.");
        delay(500);                  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("Start advertising");
        oldDeviceConnected = false;
    }

    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("Device Connected");
    }

    // delay(10000);

    // disk_space -= 0.1;
    // recordings += 1;
    // Serial.printf("Disk Space = %.2f Gb", disk_space);
    // Serial.println();
    // Serial.printf("Recordings= %d", recordings);
    // Serial.println();

    // pCharacteristic->setValue(String(disk_space).c_str());
    // pCharacteristic->notify();
}