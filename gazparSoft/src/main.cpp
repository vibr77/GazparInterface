#include <Arduino.h>

#ifndef F_CPU						// if F_CPU was not defined in Project -> Properties
#define F_CPU 8000000UL			// define it now as 8 MHz unsigned long
#endif


#include <avr/sleep.h>
//#include <util/delay.h>	

#include "Wire.h"

#include <MCP7940.h>  // Include the MCP7940 RTC library

#include <SPI.h>
#include <RF24.h>     // Include the NRF24L01 Library


/***************************************************************************************************
** Declare all program constants and enumerated types                                             **
***************************************************************************************************/
const uint8_t  SPRINTF_BUFFER_SIZE{32};  ///< Buffer size for sprintf()
const uint8_t  ALARM1_INTERVAL{15};      ///< Interval seconds for alarm
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
MCP7940_Class MCP7940;                          ///< Create instance of the MCP7940M
char          inputBuffer[SPRINTF_BUFFER_SIZE]; ///< Buffer for sprintf() / sscanf()

const int LED_BNK = 7;                          // PD7 LED on pin  10/13
const int PWR_NRF = 6;                          // PB0 POWER PIN for NRF24L01
const int NRF_CE=   9;                          // PB1 NRF24L01 CE
const int NRF_CSN=  10;                          // PB2 NRF24L01 CSN

#define BATTERYPIN A0

#define tunnel  "PIPE1"
const byte adresse[6] = tunnel;                 // Mise au format "byte array" du nom du tunnel
char message[32];                               // Avec cette librairie, on est "limité" à 32 caractères par message


RF24 radio(NRF_CE, NRF_CSN);                    // Instanciation du NRF24L01

const unsigned long KEEP_RUNNING = 10000;       //milliseconds
void blink();

volatile long gazparCounter=0;
volatile bool bAlarm=false;
volatile bool pulse=false;

void i2cScan();
void setupAlarm();
void checkAlarm();
void counter();
void sleepNow ();
void wake();

void startRadio();
void sendRadio();
void endRadio();

void startRadio(){

    pinMode(PWR_NRF, OUTPUT);
    digitalWrite(PWR_NRF, HIGH);
    delay(1500);
    radio.begin();                                // Initialisation du module NRF24
    radio.openWritingPipe(adresse);               // Ouverture du tunnel en ÉCRITURE, avec le "nom" qu'on lui a donné
    radio.setPALevel(RF24_PA_MIN);                // Sélection d'un niveau "MINIMAL" pour communiquer (pas besoin d'une forte puissance, pour nos essais)
    radio.stopListening();                        // Arrêt de l'écoute du NRF24 (signifiant qu'on va émettre, et non recevoir, ici)
}

void endRadio(){
    digitalWrite(PWR_NRF, LOW);
}

void sendRadio() {
  radio.write(&message, sizeof(message));     // Envoi de notre message
  delay(500);                                // … toutes les secondes !
}

void setupAlarm(){
    
    while (!MCP7940.begin()){
        Serial.println(F("Unable to find MCP7940. Checking again in 3s."));
        delay(3000);
    }  // of loop until device is located
    
    Serial.println(F("MCP7940 initialized."));
    while (!MCP7940.deviceStatus()) {
        Serial.println(F("Oscillator is off, turning it on."));
        bool deviceStatus = MCP7940.deviceStart();  // Start oscillator and return state
        if (!deviceStatus){
            Serial.println(F("Oscillator did not start, trying again."));
            delay(1000);
        }  // of if-then oscillator didn't start
    }    // of while the oscillator is of
  
    Serial.println("Setting MCP7940M to date/time of library compile");
    MCP7940.adjust();  // Use compile date/time to set clock

    Serial.print("Date/Time set to ");

    DateTime now = MCP7940.now();  // get the current time

    Serial.println("Setting MFP to true");
    if (MCP7940.setMFP(true)) 
        Serial.println("Successfully set MFP to true");
    else 
        Serial.println("Unable to set MFP pin.");

    int mfpStatus=MCP7940.getMFP();
    Serial.print("MFP Status:");
    Serial.println(mfpStatus);



  // Use sprintf() to pretty print date/time with leading zeroes
    sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
    
    Serial.println(inputBuffer);
    Serial.println("Setting alarm 0 for every minute at 0 seconds.");
    now = now + TimeSpan(0, 0, 0, ALARM1_INTERVAL);
    //MCP7940.setAlarm(0, matchSeconds, now - TimeSpan(0, 0, 0, now.second()),true);  // Match once a minute at 0 seconds
    MCP7940.setAlarm(0, matchSeconds, now ,true);  // Match every 15 sec
    
    Serial.print("Setting alarm 1 to go off at ");
    //now = now + TimeSpan(0, 0, 0, ALARM1_INTERVAL);  // Add interval to current time
    
    sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
    Serial.println(inputBuffer);
    now = now + TimeSpan(0, 0, 0, ALARM1_INTERVAL);  // Add interval to current time
        
    MCP7940.setAlarm(1, matchSeconds, now , true);  // Set alarm to go off then
    
    

}

void setup(void){
    //to minimize power consumption while sleeping, output pins must not source
    //or sink any current. input pins must have a defined level; a good way to
    //ensure this is to enable the internal pullup resistors.

    // for (byte i=0; i<20; i++) {    //make all pins inputs with pullups enabled
    //    pinMode(i, INPUT_PULLUP);
    // }
    Serial.begin(9600);
    Wire.begin(); 

    pinMode(LED_BNK, OUTPUT); 
    
    blink();         //make the led pin an output
    //digitalWrite(LED_BNK, LOW);        //drive it low so it doesn't source current
    i2cScan();
    setupAlarm();
    attachInterrupt (1, counter, RISING);
    attachInterrupt (0, wake, FALLING);
    //i2cScan();

   // noInterrupts ();          // make sure we don't get interrupted before we sleep
           // enables the sleep bit in the mcucr register
    //EIFR = bit (INTF0);       // clear flag for interrupt 0
   // attachInterrupt (0, inter, FALLING);  // wake up on rising edge
   // interrupts ();           // interr
   //sleepNow();
}


void blink(){

    for (int i=0; i<10; i++) {  
        digitalWrite(LED_BNK, HIGH);
        _delay_ms(100);		
        digitalWrite(LED_BNK, LOW);
        _delay_ms(100);	
    }
}

void wake (){
  sleep_disable ();         // first thing after waking from sleep
  detachInterrupt (0);
  bAlarm=true;

   // sleepNow ();
}  // end of wake

void counter(){
    noInterrupts();
    gazparCounter++;
    pulse=true;
    interrupts();
}

void sleepNow (){

  Serial.println("Zzzzzz...");
  delay(100);
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);   
  noInterrupts ();          // make sure we don't get interrupted before we sleep
  sleep_enable ();          // enables the sleep bit in the mcucr register
  //EIFR = bit (INTF0);       // clear flag for interrupt 0
  attachInterrupt (0, wake, FALLING);  // wake up on rising edge
  
  interrupts ();           // interrupts allowed now, next instruction WILL be executed
  sleep_cpu ();            // here the device is put to sleep
  
}  // end of sleepNow


void i2cScan(){
    byte error, address;
    int nDevices;
 
    Serial.println("Scanning...");
 
    nDevices = 0;
    for(address = 8; address < 127; address++ ){
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
 
        if (error == 0){
            Serial.print("I2C device found at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.print(address,HEX);
            Serial.println(" !");
 
            nDevices++;
        }
        /*else if (error == 4) {
            Serial.print("Unknow error at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address,HEX);
        }*/
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");
 
    delay(5000);
}

void checkAlarm(){
    static uint8_t secs;
    DateTime       now = MCP7940.now();  // get the current time
    if (secs != now.second())            // Output if seconds have changed
    {
        sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
        Serial.print(inputBuffer);
        secs = now.second();     // Set the counter for later comparision
        
        if (MCP7940.isAlarm(0)){
            Serial.print(" *Alarm0*");
            delay(1000);
            blink();
            MCP7940.clearAlarm(0);
        }                        // of if Alarm 0 has been triggered
        
        if (MCP7940.isAlarm(1))  // When alarm 0 is triggered
        {
            
            
            delay(1000);
            blink();
            MCP7940.clearAlarm(1);

            Serial.print(" *Alarm1* resetting to go off next at ");
            now = now + TimeSpan(0, 0, 0, ALARM1_INTERVAL);  // Add interval to current time
            sprintf(inputBuffer, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),now.hour(), now.minute(), now.second());
            Serial.print(inputBuffer);

            MCP7940.setAlarm(1, matchAll, now, true);  // Set alarm to go off in 10s again
        }                                            // of if Alarm 0 has been triggered
        Serial.println();
    }  // of if the seconds have changed
}  // of method checkAlarm()

void loop(void){
    Serial.println("looping");

    //delay(1000);                                      // Uncomment to see the pulse with the oscilloscope

    if (bAlarm==true){

        bAlarm=false;
        delay(100);
        Serial.println("cought...");
        Serial.print("garparCounter=");
        Serial.println(gazparCounter);
        
        int a = analogRead(BATTERYPIN);   delay(10); // discard first reading
        a = analogRead(BATTERYPIN);
    
        sprintf(message,"vbat=%d",a);
        Serial.println(message);
        
        blink();

        Serial.println("init radio");
        startRadio();
        sprintf(message,"counter=%ld",gazparCounter);
        
        Serial.println("send radio message");
        sendRadio();
        
        Serial.println(" end radio message");
        endRadio();

        DateTime       now = MCP7940.now();
        now = now + TimeSpan(0, 0, 0, ALARM1_INTERVAL);
        //MCP7940.setAlarm(0, matchSeconds, now - TimeSpan(0, 0, 0, now.second()),true);  // Match once a minute at 0 seconds
        MCP7940.setAlarm(0, matchSeconds, now ,true);               // resetting the alarm clears the alarm
        
    }
    if (pulse==true){
        sprintf(message,"counter=%ld",gazparCounter);
        Serial.println(message);
        int a = analogRead(BATTERYPIN);   delay(10); // discard first reading
        a = analogRead(BATTERYPIN);
    
        sprintf(message,"vbat=%d",a);
        Serial.println(message);
        pulse=false;
        
    }
    sleepNow();

}

void goToSleep(void)
{
    byte adcsra = ADCSRA;          //save the ADC Control and Status Register A
    ADCSRA = 0;                    //disable the ADC
    EICRA = _BV(ISC01);            //configure INT0 to trigger on falling edge
    EIMSK = _BV(INT0);             //enable INT0
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();                         //stop interrupts to ensure the BOD timed sequence executes as required
    sleep_enable();
    //disable brown-out detection while sleeping (20-25µA)
    uint8_t mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);
    uint8_t mcucr2 = mcucr1 & ~_BV(BODSE);
    MCUCR = mcucr1;
    MCUCR = mcucr2;
    //sleep_bod_disable();           //for AVR-GCC 4.3.3 and later, this is equivalent to the previous 4 lines of code
    sei();                         //ensure interrupts enabled so we can wake up again
    sleep_cpu();                   //go to sleep
    sleep_disable();               //wake up here
    ADCSRA = adcsra;               //restore ADCSRA
}

//external interrupt 0 wakes the MCU
//ISR(INT1_vect)
//{
//++gazparCounter;
   // Serial.println("External Interrupt");
//EIMSK = 0;                     //disable external interrupts (only need one to wake up)
//  }