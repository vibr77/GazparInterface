/*
__   _____ ___ ___        Author: Vincent BESSON
 \ \ / /_ _| _ ) _ \      Release: 0.52
  \ V / | || _ \   /      Date: 20240228
   \_/ |___|___/_|_\      Description: Gazpar Pulse Counter ATMEGA328P with NRF24L01 Module
                2024      Licence: Creative Commons
______________________

Revision:
+ 20240301 add EEPROM Mgt


// EEPROM Memory location 

10 = Counter long       size    16

*/




#include <Arduino.h>
#include <ArduinoLog.h>                         // Logging lib
#include <avr/sleep.h>                          // Deepsleep lib
#include "Wire.h"                   
#include <MCP7940.h>                            // Include the MCP7940 RTC library
#include <SPI.h>
#include <RF24.h>                               // Include the NRF24L01 Library

#include <extEEPROM.h>                          //https://github.com/PaoloP74/extEEPROM

extEEPROM myEEPROM(kbits_16, 1, 16, 0x50);


/* <!> DEBUG TESTING MODE */

#define DEBUG


/***************************************************************************************************
** Declare all program constants and enumerated types                                             **
***************************************************************************************************/

const uint8_t  SPRINTF_BUFFER_SIZE{32};         //< Buffer size for sprintf()

// ALARM 1 DELAY MGT
const uint8_t  ALARM1_INTERVAL_HOUR{0};         //< Interval HOUR for alarm


#ifdef DEBUG
const uint8_t  ALARM1_INTERVAL_MIN{1};          //< Interval MIN for alarm
#elif
const uint8_t  ALARM1_INTERVAL_MIN{30};       //< Interval MIN for alarm
#endif

const uint8_t  ALARM1_INTERVAL_SEC{0};          //< Interval SEC for alarm

#define SERIAL_LOG                              // Enable serial logging
                                 // DEBUG MODE

/*! ///< Enumeration of MCP7940 alarm types */
enum alarmTypes {
  matchSeconds,
  matchMinutes,
  matchHours,
  matchDayOfWeek,
  matchDayOfMonth,
  Unused1,
  Unused2,
  matchAll,
  Unknown
};
MCP7940_Class MCP7940;                              // < Create instance of the MCP7940M

char inputBuffer[SPRINTF_BUFFER_SIZE];              // < Buffer for sprintf() / sscanf()

const int ACT_LED   = 5;                            // PD5 Activity LED pin 11 
const int WAKE_LED  = 7;                            // PD7 Wake up LED on pin 13
const int PWR_NRF   = 6;                            // PB0 POWER PIN for NRF24L01
const int NRF_CE    = 9;                            // PB1 NRF24L01 CE
const int NRF_CSN   = 10;                           // PB2 NRF24L01 CSN

#define BATTERYPIN A0
//#define tunnel  "D6E1A"                           // PROD 5 char Tunnel definition for NRF24
#define tunnel  "PIPE1"                             // TEST Tunnel

const byte adresse[6] = tunnel;                     // Mise au format "byte array" du nom du tunnel
char radioMessage[32];                               // Avec cette librairie, on est "limité" à 32 caractères par message

RF24 radio(NRF_CE, NRF_CSN);                        // Instanciation du NRF24L01

volatile unsigned long gazparCounter =0;            // Pulse counter
volatile unsigned long measuredvbat  =0;            // vBat
volatile unsigned long elapse=0;                    // counter of RTC wakeup cycle

volatile bool bAlarm=false;                         // boolean Alarm trigger flag
volatile bool bPulse=false;                         // boolean Pulse received flag

#define disk1 0x50                                  //Address of eeprom chip

/***************************************************************************************************
** HEADER                                                                                         **
***************************************************************************************************/

void writeLongEeprom(int addr,long value);
long readLongEeprom(int addr);

void flashActivityLed();
void blinkWakeUpLed();

void i2cScan();
void setupAlarm();

void wake();
void counter();
void sleepNow();

int batteryPercent(int a);
void startRadio();
void sendRadio();
void endRadio();

/***************************************************************************************************
** FUNCTIONS                                                                                      **
***************************************************************************************************/


void writeLongEeprom(int addr,long value){
    char str[16];
    sprintf(str,"%ld",value);
    byte i2cStat = myEEPROM.write(addr,(byte *)str,16);
    return;
}

long readLongEeprom(int addr){
    char str[16];
    byte i2cStat = myEEPROM.read(addr,(byte *)str,16);
    long ret=atol(str);
    return ret;
}


int batteryPercent(int a){
    Log.notice("vbat=%d" CR,a);
    int max =1023;
    int floor=750;
    int percent=100-((max-a)*100)/(max-floor);
    return percent;

}

void startRadio(){
    pinMode(PWR_NRF, OUTPUT);
    digitalWrite(PWR_NRF, HIGH);
    delay(1500);                                    // Wait to init the Power on the module (need tweaking)
    radio.begin();                                  // Initialisation du module NRF24
    radio.openWritingPipe(adresse);                 // Ouverture du tunnel en ÉCRITURE, avec le "nom" qu'on lui a donné
    radio.setPALevel(RF24_PA_HIGH);
    radio.setAutoAck(true);                         // Important <!>              
    radio.stopListening();                          // Arrêt de l'écoute du NRF24 (signifiant qu'on va émettre, et non recevoir, ici)
}

void endRadio(){
    digitalWrite(PWR_NRF, LOW);
}

void sendRadio() {
    radio.write(&radioMessage, sizeof(radioMessage));         // Radio Message sent
    delay(500);                                     
}

void setupAlarm(){                                                                          // 
    
    while (!MCP7940.begin()){
        Log.warning("Unable to find MCP7940. Checking again in 3s." CR );
        delay(3000);
    }

    Log.notice("MCP7940 initialized." CR);
    while (!MCP7940.deviceStatus()) {
        Log.notice("Oscillator is off, turning it on." CR);
        bool deviceStatus = MCP7940.deviceStart();  // Start oscillator and return state
        if (!deviceStatus){
            Log.notice("Oscillator did not start, trying again." CR);
            delay(1000);
        }  
    }
  
    Log.notice("Setting MCP7940M to date/time of library compile" CR);
    MCP7940.adjust();                                                       // Use compile date/time to set clock

    DateTime now = MCP7940.now();                                           // get the current time

    sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
    Log.notice("Date/Time set: %s" CR ,inputBuffer);

    Log.notice("Setting MFP to true" CR);
    if (MCP7940.setMFP(true))                                               // set MFP true
        Log.notice("Successfully set MFP to true" CR);
    else 
        Log.notice("Unable to set MFP pin." CR);

    int mfpStatus=MCP7940.getMFP();                                        // gettting the status
    
    Log.notice("MFP Status: %d" CR,mfpStatus);

    // Setting initial Alarm 1
    now = now + TimeSpan(0, ALARM1_INTERVAL_HOUR, ALARM1_INTERVAL_MIN, ALARM1_INTERVAL_SEC);
    sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
    Log.notice("Setting alarm 1 at: %s " CR,inputBuffer);
    MCP7940.setAlarm(1, matchAll, now , true);

}

void flashActivityLed(){                                                    // flashing the activity LED
    digitalWrite(ACT_LED, HIGH);
    _delay_ms(100);		
    digitalWrite(ACT_LED, LOW);
    _delay_ms(100);

}

void blinkWakeUpLed(){                                                      // 10x blinking Wake up LED 
    for (int i=0; i<10; i++) {  
        digitalWrite(WAKE_LED, HIGH);
        _delay_ms(100);		
        digitalWrite(WAKE_LED, LOW);
        _delay_ms(100);	
    }

}

void wake (){
    sleep_disable ();                                                       // first thing after waking from sleep
    detachInterrupt (0);
    bAlarm=true;

}

void counter(){                                                             // Pulse counter function
    noInterrupts();
    gazparCounter++;
    bPulse=true;
    interrupts();

}

void sleepNow (){
    Log.notice("Zzzzzzz...." CR );

    delay(100);
    set_sleep_mode (SLEEP_MODE_PWR_DOWN);   
    noInterrupts ();                            // make sure we don't get interrupted before we sleep
    sleep_enable ();                            // enables the sleep bit in the mcucr register
    //EIFR = bit (INTF0);                       // clear flag for interrupt 0
    attachInterrupt (0, wake, FALLING);         // wake up on rising edge
    
    interrupts ();                              // interrupts allowed now, next instruction WILL be executed
    sleep_cpu ();                               // here the device is put to sleep
    
}

void i2cScan(){
    byte error, address;
    int nDevices;
 
    Log.notice("i2cScan(): Scanning..." CR );
    nDevices = 0;
    for(address = 8; address < 127; address++ ){
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0){
            Log.notice("i2cScan(): device found at address 0x%X" CR,address ); 
            nDevices++;
        }
    }

    if (nDevices == 0)
        Log.notice("i2cScan(): no devices found" CR );
    else
        Log.notice("i2cScan(): done" CR );
 
    delay(5000);

}
        

void setup(void){

    //to minimize power consumption while sleeping, output pins must not source
    //or sink any current. input pins must have a defined level; a good way to
    //ensure this is to enable the internal pullup resistors.

    for (byte i=0; i<20; i++) {    //make all pins inputs with pullups enabled
        pinMode(i, INPUT_PULLUP);
    }

    Serial.begin(9600);
    
    Log.begin(LOG_LEVEL_VERBOSE, &Serial);
    Log.setShowLevel(false);                                                        // Do not show loglevel, we will do this in the prefix

    pinMode(WAKE_LED, OUTPUT);
    pinMode(ACT_LED, OUTPUT);

    blinkWakeUpLed();                                                               //Blink at startup
   
    Wire.begin();                                                                   // I2C bus startup 
    i2cScan();                                                                      // scan the I2C bus 
    setupAlarm();                                                                   // setup the initial alarm & the RTC module
    attachInterrupt (1, counter, RISING);                                           // attached pulse counter interrupt on 
    attachInterrupt (0, wake, FALLING);

    analogReference(INTERNAL);                                                      // for ADC reading INTERNAL 1.1v will be used 
                                                                                    // <!> AREF should not be connected otherwise short is made in ATMEGA
    byte i2cStat = myEEPROM.begin();                                                // Initialize the EEPROM
    if ( i2cStat != 0 ) {
        Log.error("EEPROM is not ready");
    }
    gazparCounter=readLongEeprom(10);                                               // Read EEPROM at location 10
    Log.notice("EEPROM read gazparCounter=%l" CR,gazparCounter);
    
    //gazparCounter=0;                                                                // Reinit
    
    //i2cStat = myEEPROM.write(42, 16);
    //delay(50);

    // Let's make a single read
    //int readValue = myEEPROM.read(42);
    //delay(50);
    //Log.notice("reading value from the eeprom at position 42: %d" CR,readValue);
    //char * str1=(char *)malloc(32*sizeof(char));
    //char str1[32];
    //sprintf(str1,"Wahoo this rock");
    
    //i2cStat = myEEPROM.write(10,(byte*)str1,32);
    //delay(50);
    //char * str2=(char *)malloc(32*sizeof(char));
    //char str2[32];
    //i2cStat = myEEPROM.read(10,(byte *)str2,32);
    //Log.notice("Res:%s"CR, str2);
    //writeLongEeprom(10,6789860);

                                     
    //Log.notice("Res:%l"CR, a);
    bAlarm=true;
}

void loop(void){
    Log.notice("looping" CR);
  
    if (bAlarm==true){
        elapse++;
        bAlarm=false;

        delay(100);
        blinkWakeUpLed();                                                           // Blink Wakeup led

        measuredvbat = analogRead(BATTERYPIN); delay(100);                          // discard first reading
        measuredvbat = analogRead(BATTERYPIN);                                      // keep second reading
        measuredvbat = batteryPercent(measuredvbat);                                // Convert to battery percent

        Log.notice("write EEPROM current counter");
        writeLongEeprom(10,gazparCounter);
        
        Log.notice("Radio start" CR);
        startRadio();                                                               // start Radio module NRFL01
        sprintf(radioMessage,"d:%lu;v:%d;p:%lu;",elapse,measuredvbat,gazparCounter);   // <!> Warning Radio Message can not exceed 32 char, after 32 it is discarded
        Log.notice("Radio message sent: %s" CR,radioMessage);
        sendRadio();
        
        Log.notice("Radio end" CR);
        endRadio();

        // Setting next Alarm
        DateTime now = MCP7940.now();
        now = now + TimeSpan(0, ALARM1_INTERVAL_HOUR, ALARM1_INTERVAL_MIN, ALARM1_INTERVAL_SEC);
        sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
        Log.notice("Setting alarm 1 at: %s " CR,inputBuffer);
        MCP7940.setAlarm(1, matchAll, now , true);
    }

    if (bPulse==true){
        flashActivityLed();
#ifdef DEBUG
        Log.notice("counter=%l" CR,gazparCounter);
        int a = analogRead(BATTERYPIN);   
        delay(100); // discard first reading
        a = analogRead(BATTERYPIN);
        a=batteryPercent(a);
        Log.notice("vbat=%d" CR,a);
#endif
        bPulse=false; 
    }

    sleepNow();

}