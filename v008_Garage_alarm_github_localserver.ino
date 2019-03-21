//v008 Garage alarm (change from interrupts to polling)
//
//
//Project requirements:
//The alarm panel has two outputs, Armed and Sounder
//Poll these two outputs every 1000ms and write the result to a virtual LED (Blynk app on phone)
//The alarm panel has one input SET which can arm or disarm the system
//When a virtual button is pressed (on phone Blynk app) the alarm is armed or unarmed.
//
//
//Changelog:
//v004 had issues with Blynk server heartbeat timeouts and app disconnecting and sometimes unresponsive
//v005 combines both armed and sounder functions into one, run by one timer
//v006 added serial to terminal code as per this thread https://community.blynk.cc/t/serial-arduino-to-terminal-blynk/31547/4
//v007 added correct serial to terminal code working correctly - needs rx to tx link on device
//v008 added date/time stamp to terminal widget, required rtc widget on app, and added serial prints when states change



#define BLYNK_PRINT Serial // Defines the object that is used for printing Blynk stuff
#include <BlynkSimpleEsp8266.h>
#include <ESP8266mDNS.h>  //for OTA updates
#include <WiFiUdp.h>     //for OTA updates
#include <ArduinoOTA.h>  //for OTA updates

#include <WidgetRTC.h>
  
SimpleTimer timer; //setup simple timer to call functions on a timed basis from void loop

//Blynk credentials
char auth[] = ""; //ENTER YOUR AUTH TOKEN HERE
//Wifi credentials
char ssid[] = "";  //ENTER WIFI NAME HERE
char pass[] = "";  //ENTER WIFI PASSWORD HERE
//Server credentials
char server[] = "192.X.X.X"; //ENTER SEERVER IP ADDRESS HERE - LOCAL SERVER ONLY
int port = 8080;  //ENTER PORT HERE - LOCAL SERVER ONLY

//Support for Blynk terminal
WidgetTerminal terminal(V1); //terminal reads from virtual pin specified

//Support for Blynk real time clock
WidgetRTC rtc;

//Setup constants for the sketch
const byte Armed = D5;    // INPUT - is the panel armed, or un-armed/alarmed? (armed = HIGH/3.3V and Unarmed/alarmed = LOW/0V)
const byte Sounder = D6;  // INPUT - is the sounder on or off? (Sounder on = LOW/0.33V and Sounder off = HIGH/3.3V)
const byte SET = D4;      // OUTPUT - set or unset the alarm (HIGH = unset the alarm,  LOW = set the alarm)

//Setup variables for Armed
int ArmedState = digitalRead(Armed); //reads armed state of the alarm (armed = HIGH/3.3V and Unarmed/alarmed = LOW/0V)
int lastArmedState = ArmedState; // the previous read from the input pin

//Setup variables for Sounder
int SounderState = digitalRead(Sounder); //reads state of sounder i.e on or off (Sounder on = LOW/0.33V and Sounder off = HIGH/3.3V)
int lastSounderState = SounderState; // the previous read from the input pin

//Setup variables for debouncing of inputs
unsigned long lastArmedDebounceTime = 0;    //setup debounce variable for checkArmed function
unsigned long lastSounderDebounceTime = 0;  // setup debounce variable for checkSounder function
unsigned long debounceDelay = 50;           // the global debounce time in ms, increase if debounce issues continue

//Setup variable for Blynk virtual pin
static unsigned long last_interrupt_time = 0;
bool LastVirtualButtonState = 0;  //"0","FALSE","LOW' means exactly the same




void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass, server, port);  //connects to Wifi and LOCAL Blynk server (running on raspberry pi)
  setSyncInterval(10 * 60); // Sync Blynk real time clock (RTC) interval in seconds (10 minutes)
 
  //OTA stuff (update device from Arduino IDE remotely)
  ArduinoOTA.onError([](ota_error_t error) { ESP.restart(); });
  ArduinoOTA.setHostname("INSERT_NAME_HERE");  // INSERT DEVICE NAME HERE
  ArduinoOTA.begin();
  
  //Setup the previously assigned constants
  pinMode(Armed, INPUT);       //is the panel armed, or un-armed/alarmed? (armed = HIGH/3.3V and Unarmed/alarmed = LOW/0V)
  pinMode(Sounder, INPUT);     //is the sounder on or off? (Sounder on = LOW/0.33V and Sounder off = HIGH/3.3V)
  pinMode(SET, OUTPUT);        //set or unset the alarm (HIGH = unset the alarm,  LOW = set the alarm)
  digitalWrite(SET, LOW);      //ensures the alarm defaults to SET condition after power loss of Wemos

  //write the current states to the Blynk app
  Blynk.virtualWrite(V5, (ArmedState * 255));      // writes set or unset state of alarm to Blynk virtual LED pin V5
  Blynk.virtualWrite(V6, (!SounderState * 255));   //writes sounder on or off state to Blynk virtual LED pin V6 (inverted logic as sounder is on when at 0V


  timer.setInterval(100L, checkCombined);  //Setup a function to be called every second

  //Serial to terminal setup
   Serial.println("------terminal-------");
   serial_print_date_time(); //call function to print date and time stamp
   Serial.println("Startup");
   
   timer.setInterval(100L, Sent_serial);  // Run every 100ms
  
}



void loop()
{
  Blynk.run();  //This function should be called frequently to process incoming commands and perform housekeeping of Blynk connection.
  timer.run();  //Initiates SimpleTimer to runs timed functions
  ArduinoOTA.handle();  // For OTA
}


BLYNK_CONNECTED()
{
  // Synchronize time on connection
  rtc.begin();
}


//A function to print current date and time to Blynk terminal widget, gets called from the below functions
void serial_print_date_time()
{
  String currentDate = String(day()) + "/" + month() + "/" + year();
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  Serial.print(currentDate);
  Serial.print(" @ ");
  Serial.print(currentTime);
  Serial.print(" : ");
}



void Sent_serial() //A function which reads the serial monitor output and send to terminal widget (requires tx and rx pins to be connected together on device).
{
  // Sent serial data to Blynk terminal - Unlimited string readed
  String content = "";  //null string constant ( an empty string )
  char character;
  while(Serial.available())
  {
   character = Serial.read();
   content.concat(character);
  }
  if (content != "") 
  {
   Blynk.virtualWrite (V1, content);
  }  
}



void checkCombined()// a combined function to read the "armed" state and the "sounder" state
{
 int readingArmed = digitalRead(Armed); // read the state of "Armed" into a local variable:

  if (readingArmed != lastArmedState)   //has the state changed?
  {
    lastArmedDebounceTime = millis();  // if yes(state has changed), reset the debouncing timer to the current millis
  }


  if ((millis() - lastArmedDebounceTime) > debounceDelay) // whatever readingArmed is at, it's been there for longer than the debounce delay, so take it as the actual current state
  {

    if (readingArmed != ArmedState) // has the armed state has changed?
    {
        ArmedState = readingArmed;  // if yes(state has changed) 
        Blynk.virtualWrite(V5, (ArmedState) * 255); // writes ArmedState to Blnk V5 virtual LED names "Alarm armed?"

        if (ArmedState == LOW)
        {
          serial_print_date_time();
          Serial.println("System disarmed"); //send to serial print which is forwarded to terminal
        }

        else
        {
          serial_print_date_time();
          Serial.println("System armed"); //send to serial print which is forwarded to terminal
        }
      
    }
  }
  
  lastArmedState = readingArmed; // save the readingArmed. Next time through the function, it'll be the lastArmedState:

 int readingSounder = digitalRead(Sounder); // read the state of "Armed" into a local variable:

  if (readingSounder != lastSounderState)   //has the state changed?
  {
    lastSounderDebounceTime = millis();  // if yes(state has changed), reset the debouncing timer to the current millis
  }


  if ((millis() - lastSounderDebounceTime) > debounceDelay) // whatever readingSounder is at, it's been there for longer than the debounce delay, so take it as the actual current state
  {

    if (readingSounder != SounderState) // has the sounder state has changed?
    {
          SounderState = readingSounder;  // if yes(state has changed) 
          Blynk.virtualWrite(V6, (!SounderState) * 255); // writes SounderState to Blnk V6 virtual LED named "Sounder on?"

          if (SounderState == LOW)
          {
          Blynk.notify("Garage alarm is sounding!");  //only send Blynk app notification when then sounder is ON
          serial_print_date_time();
          Serial.println("Sounder activated!"); //send to serial print which is forwarded to terminal
          }

          else
          {
          serial_print_date_time();
          Serial.println("Sounder deactivated"); //send to serial print which is forwarded to terminal
          }
     }
  }
  
  lastSounderState = readingSounder; // save the readingSounder. Next time through the function, it'll be the lastSounderState:
}



// BLYNK_WRITE is a function called every time the device gets an update of a Virtual Pin value from the server (e.g. Blynk app virtual button is pressed)
// contains "latching" code to stop long hold being registered as repeated presses.
BLYNK_WRITE(V3)
{
  int VirtualButtonState = param.asInt(); // assigning incoming value from pin V3 to a variable

  if ((VirtualButtonState) && (!LastVirtualButtonState)) // "VirtualButtonState" is the Blynk virtual button current state ||||||  this means same as "if ((VirtualButtonState == 1) && (LastVirtualButtonState == 0))"
    //if V3 virtual button is still being pressed, the LastVirtualState is set to 1, and !LastVirtualState will therefore be 0. Hence 1 && 0 condition == 0 and therefore function will not be called.
  {
    digitalWrite(SET, !digitalRead(SET));       //writes the inverse value to the pin  (booleon NOT operator )
    Blynk.virtualWrite(V0, digitalRead(SET) * 255);  // for  information only, writes the state of the keyswitch SET contacts to Blynk virtual LED at V0

    if (digitalRead(SET) == LOW)
    {
     serial_print_date_time();
     Serial.println("System arm request"); //send to serial print which is forwarded to terminal
    }

    else
    {
     serial_print_date_time();
     Serial.println("System disarm request"); //send to serial print which is forwarded to terminal
    }
  }

  LastVirtualButtonState = VirtualButtonState;  // sets LastVirtualButtonState to the same as pinValue, so if pinValue (V3 button) is high, LastVirtualPinState gets set to high
}
