# esp-now-admin-monitor
This will be the monitoring application for the audio guestbook. It will act as an ESP NOW server and transmit (encrypted) data to the companion application running on an Waveshare ESP32-S3 AMOLED 1.43" round display.

All communication between the Teensy and the ESP will be via UART and one way only, Teensy->ESP, the admin program is not designed to query/control the audio guestbook.
