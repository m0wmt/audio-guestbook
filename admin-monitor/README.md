# admin-monitor
This will be the monitoring application for the audio guestbook. It will contain a web page giving up-to-date information/status of the audio guestbook via wi-fi for admin users. The web page will only be availabe if the audio guestbook is powered by mains due to the wi-fi power requirements of the ESP32 board, i.e. a battery won't last very long using wi-fi.

All communication between the Teensy and the ESP will be via UART and one way only, Teensy->ESP, the admin program is not designed to query/control the audio guestbook.