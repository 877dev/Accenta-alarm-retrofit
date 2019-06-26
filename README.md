# Accenta-alarm-retrofit
Accenta intruder alarm retrofit with Wemos D1 mini arduino and Blynk - could be done to any suitable alarm panel though.

This project is to retrofit an alarm panel with Blynk app control and notifications.

The alarm can be armed or disarmed via the top right button of the app.
The two LED's show if the alarm or sounder are active.
When the sounder becomes active, an in app notification shows on the phone.

You will need:
-------------

Wemos D1 mini

Suitable 5V psu for the Wemos

Protoboard or stripboard

Resistors to create voltage divider to make 3.3V imputs

NPN transistor

PCB mounted terminals

Cables



Instructions:
-------------

Measure the voltages at your alarm panel terminals in the disarmed/armed/alarmed states.

Note which terminals change voltages in the different states.

Determine the resistor values to provide the correct 3.3V to D5/D6 http://www.ti.com/download/kbase/volt/volt_div3.htm

Breadboard the project to test it out. 

Download Blynk app for iOS/Android. 

Use the included QR code to get the project, note your auth token which will be emailed to you. 

Program the Wemos using Arduino IDE, use the Blynk server .ino  (local server for advanced users with their own server), remember to change the auth token and wifi details. 

Test it all works as you want. 

Solder your project to a permanent stripboard/protoboard and fit to your alarm panel.
