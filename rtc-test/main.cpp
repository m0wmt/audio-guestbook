/*
 * Teensy example code illustrating Time library with Real Time Clock.
 *
 * RTC clock is set in response to serial port time message 
 * On Linux, you can use "date +T%s > /dev/ttyACM0" (UTC time zone) to 
 * set the time once the program is running.  If you have a button battery
 * connected to the RTC chip on the Teensy it will keep the time, not sure
 * for how long though.
 */

 #include <Arduino.h>
 #include <TimeLib.h>
 
 //#define TIME_HEADER  "T"    // Header tag for serial time sync message
 
 int blinkLed = 500;        // Blink led every n miliseconds
 int ledState = LOW;
 static int oneSecond = 1000;
 
 //static unsigned long processSyncMessage(void);
 static void printDigits(int digits);
 static void digitalClockDisplay(void);
 static void blink_led(void);
 static void print_time(void);
 
 time_t getTeensy3Time()
 {
   return Teensy3Clock.get();
 }
 
 void setup() {
     pinMode(LED_BUILTIN, OUTPUT);     // Orange LED on board
     digitalWrite(LED_BUILTIN, HIGH);  // Glowing whilst running through setup code
 
     while (!Serial) {
        // Wait for serial to start!
     } 
 
     if (CrashReport)
         Serial.print(CrashReport);
     else {
         Serial.println("No crash report - all ok.");
         Serial.println("");
     }    
 
     setSyncProvider(getTeensy3Time);   // the function to get the time from the RTC
     if (timeStatus() != timeSet) 
         Serial.println("Unable to sync with the RTC");
     else
         Serial.println("RTC has set the system time");     
 
     digitalWrite(LED_BUILTIN, LOW);   // Turn LED off
 }
 
 void loop() {
     // if (Serial.available()) {
     //     time_t t = processSyncMessage();
     //     if (t != 0) {
     //         RTC.set(t);   // set the RTC and the system time to the received value
     //         setTime(t);    
     //         Teensy3Clock.set(now());      
     //     }
     // }
     // digitalClockDisplay();
     // delay(1000);
     print_time();
     blink_led();
 }
 
 // static unsigned long processSyncMessage(void) {
 //     unsigned long pctime = 0L;
 //     const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 
 
 //     if(Serial.find(TIME_HEADER)) {
 //         pctime = Serial.parseInt();
 //         return pctime;
 //         if( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
 //           pctime = 0L; // return 0 to indicate that the time is not valid
 //         }
 //     }
 //     return pctime;
 // }
 
 static void printDigits(int digits) {  
     // utility function for digital clock display: prints preceding colon and leading 0
     Serial.print(":");
     if(digits < 10)
         Serial.print('0');
 
     Serial.print(digits);
 }
 
 static void digitalClockDisplay(void) {
     // digital clock display of the time
     Serial.print(hour());
     printDigits(minute());
     printDigits(second());
     Serial.print(" ");
     Serial.print(day());
     Serial.print(" ");
     Serial.print(month());
     Serial.print(" ");
     Serial.print(year()); 
     Serial.println(); 
 }
 
 static void blink_led(void) {
     static unsigned long previousMillis;
     unsigned long timeNow = millis();
 
     if (timeNow - previousMillis >= blinkLed) {
         previousMillis = timeNow;
         if (ledState == LOW) {
             ledState = HIGH;
         } else {
             ledState = LOW;
         }
 
         digitalWrite(LED_BUILTIN, ledState);
     }
 }
 
 static void print_time(void) {
     static unsigned long previousTimeInMillis;
     unsigned long timeNow = millis();
 
     if (timeNow - previousTimeInMillis >= oneSecond) {
         previousTimeInMillis = timeNow;
 
         digitalClockDisplay();
     }
 }