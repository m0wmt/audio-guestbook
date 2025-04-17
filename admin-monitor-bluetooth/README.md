# admin-monitor-bluetooth
This will be the monitoring application for the audio guestbook. It will contain a web page giving up-to-date information/status of the audio guestbook via bluetooth for admin users. It is hoped that this web page will be availabe when the audio guestbook is powered by battery. If we have access to mains power then the wi-fi admin option will be used.

All communication between the Teensy and the ESP will be via UART and one way only, Teensy->ESP, for secuirty the admin program is not designed to query/control the audio guestbook.