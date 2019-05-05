//v013 Garage alarm (see changelog below)
//
//
//Project requirements:
//The alarm panel has two outputs, Armed and Sounder
//Poll these two outputs every 1000ms and write the results to two virtual LED's (Blynk app on phone)
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
//v009 fixes for terminal repeating (used BlynkTimer and increased timer interval. Optional add Blynk.run to function so that more CPU time is allocated for housekeeping. Rewrote serial prints as virtual writes to V1. Search for "v009" to see changes.
//v010 added delay between timer functions. More fixes for terminal repeats (changed Blynk.virtualWrite for Serial.print).
//v011 changed send_terminal function as per PeteKnight (Blynk forum), changed terminal prints to serial prints. Removed unneeded comments.
//v012 updated serial_print_date_time to force 2 decimal places, added button v2 to clear terminal (credit PeteKnight)
//v012b reverted back to cloud server
//v013 commented out "void keep_terminal_awake()" function to test new Blynk app beta 2.26.0(6)  fixes for terminal repeats/clears
//
//Optional to do list:
//Fix to ensure time synch occured before printing date/time ins BLYNK_CONNECTED() function
//Fix order initial prints are made to terminal (BLYNK_CONNECTED() being called at same time as void setup() serial prints ?
//


//#define BLYNK_DEBUG  //optional, enable for debugging to serial 

#define BLYNK_PRINT Serial // Defines the object that is used for printing Blynk stuff
#include <BlynkSimpleEsp8266.h>
#include <ESP8266mDNS.h>  //for OTA updates
#include <WiFiUdp.h>     //for OTA updates
#include <ArduinoOTA.h>  //for OTA updates
#include <WidgetRTC.h>   //for clock sync
#include <TimeLib.h> // check if needed
  

//Blynk credentials
char auth[] = ""; // Enter the Auth code which was send by Blynk


//Wifi credentials
char ssid[] = "";  //Enter your WIFI Name
char pass[] = "";  //Enter your WIFI Password

//Server credentials
//char server[] = "192.168.1.95"; //LOCAL SERVER ONLY
//int port = 8080;  //LOCAL SERVER ONLY


//Support for Blynk terminal
WidgetTerminal terminal(V1); //terminal reads from virtual pin specified, possibly not needed when using "Blynk.virtualWrite(Vx, content)"
bool terminal_output_enabled = true;

//Support for Blynk real time clock
WidgetRTC rtc;

//SimpleTimer timer; //setup simple timer to call functions on a timed basis from void loop
BlynkTimer timer; // v009 improvement http://docs.blynk.cc/#blynk-firmware-blynktimer 


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
  Serial.setRxBufferSize(1024); // Increase the serial buffer size //v011 improvement
  //Blynk.begin(auth, ssid, pass, server, port);  //connects to Wifi and LOCAL Blynk server (running on raspberry pi)
  Blynk.begin(auth, ssid, pass); // cloud server

  setSyncInterval(60 * 60); // v009 increase time if needed - Sync Blynk real time clock (RTC) interval in seconds (default 10x60 = 10 minutes)
 
  //OTA stuff (update device from Arduino IDE remotely)
  ArduinoOTA.onError([](ota_error_t error) { ESP.restart(); });
  ArduinoOTA.setHostname("YOUR_HOST_NAME_HERE");     // for live system
  ArduinoOTA.begin();
  
  //Setup the previously assigned constants
  pinMode(Armed, INPUT);       //is the panel armed, or un-armed/alarmed? (armed = HIGH/3.3V and Unarmed/alarmed = LOW/0V)
  pinMode(Sounder, INPUT);     //is the sounder on or off? (Sounder on = LOW/0.33V and Sounder off = HIGH/3.3V)
  pinMode(SET, OUTPUT);        //set or unset the alarm (HIGH = unset the alarm,  LOW = set the alarm)
  digitalWrite(SET, LOW);      //ensures the alarm defaults to SET condition after power loss of Wemos

  //write the current states to the Blynk app
  Blynk.virtualWrite(V5, (ArmedState * 255));      // writes set or unset state of alarm to Blynk virtual LED pin V5
  Blynk.virtualWrite(V6, (!SounderState * 255));   //writes sounder on or off state to Blynk virtual LED pin V6 (inverted logic as sounder is on when at 0V

  //Setup Blynktimers
  timer.setInterval(1000L, Send_Serial);    // Timer calls function to send any serial data to terminal - default 100ms  - v009 increase interval if needed //v011 increased to 1000ms
  delay(500);                               //add delay to ensure timer functions do not get called at the same time
  timer.setInterval(1000L, checkCombined);  //Timer calls function to read state of alarm panel - default 100ms  - v009 increase interval if needed //v011 increased to 1000ms
  //delay(250);                               //add delay to ensure timer functions do not get called at the same time  
  //timer.setInterval(8000L, keep_terminal_awake);  // Write to the terminal widget every 8 seconds to stop repeated prints (temp bug fix)
  // do not exceed 10 seconds, it stops working for some reason....

  //Optional print free memory to terminal
   Serial.println("v013 Garage alarm");   // Your description here
   Serial.print("Free RAM: ");      
   Serial.print(ESP.getFreeHeap()); 
   Serial.println(" bytes");     
}



void loop()
{
  Blynk.run();  //This function should be called frequently to process incoming commands and perform housekeeping of Blynk connection.
  timer.run();  //Initiates SimpleTimer to runs timed functions
  ArduinoOTA.handle();  // For OTA
}




// a function called every 10 seconds to keep terminal awake, to prevent duplicate entries
//void keep_terminal_awake()
//{
//  Blynk.virtualWrite(V1, "\b"); // Write nothing (backspace) to the terminal widget on V1
//}


BLYNK_CONNECTED()
{
  rtc.begin();  // Synchronize time on connection
  serial_print_date_time();
  Serial.println("Server connected");     
}



//NEW IMPROVED TIME/DATE FUNCTION by PeteKnight
//A function to print current date and time to Blynk terminal widget.
// Uses string print (sprintf) to print to char buffer, which then gets printed to serial via Serial.print
void serial_print_date_time()
{
  char date_time [22];   
  sprintf (date_time, "%02d/%02d/%04d @ %02d:%02d:%02d ", day(), month(), year(),hour(), minute(), second()        );
  Serial.print(date_time);
}




//New function by PeteKnight (Blynk forum)
  void Send_Serial()
  {
    // Sends serial data to Blynk as well as the serial monitor (handy for setups where the MCU isn't connected to serial because OTA is being used)
    // Note that a jumper is needed between Tx and Rx, which needs to be removed if doing a serial flash upload (but this is not necessary for OTA flash upload)
  
    if (terminal_output_enabled)
    {    
      String content = "";
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
  } //end of void Send_Serial



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
          Serial.println("System disarmed"); //v010
        }

        else
        {
          serial_print_date_time();
          Serial.println("System armed"); //v010
        }
    }
  }
  
  lastArmedState = readingArmed; // save the readingArmed. Next time through the function, it'll be the lastArmedState:

 Blynk.run(); //v009 optional extra Blynk.run to allocate more CPU/RAM to the Blynk process, to remove duplicate terminal issues

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
          Serial.println("Sounder activated!"); //v010
          }

          else
          {
          serial_print_date_time();
          Serial.println("Sounder deactivated"); //v010
          }
        
     }
  }
  
  lastSounderState = readingSounder; // save the readingSounder. Next time through the function, it'll be the lastSounderState:
}


// A function where pressing the app button "Clear terminal" will of course clear the terminal output
BLYNK_WRITE(V2)    // Momentary button to clear the terminal
{
  if (param.asInt())
  {    
    Blynk.virtualWrite(V1, "clr");  // Clear the terminal content
  }
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
     Serial.println("System arm request"); //v010
    }

    else
    {
     serial_print_date_time();
     Serial.println("System disarm request"); //v010
    }
  }

  LastVirtualButtonState = VirtualButtonState;  // sets LastVirtualButtonState to the same as pinValue, so if pinValue (V3 button) is high, LastVirtualPinState gets set to high
}
