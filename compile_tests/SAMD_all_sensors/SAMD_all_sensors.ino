/*****************************************************************************
logging_to_EnviroDIY_Zero.ino
Written By:  Sara Damiano (sdamiano@stroudcenter.org)
Development Environment: PlatformIO
Hardware Platform: Adafruit Adalogger M0
Software License: BSD-3.
  Copyright (c) 2017, Stroud Water Research Center (SWRC)
  and the EnviroDIY Development Team

This sketch is an example of logging data to an SD card and sending the data to
the EnviroDIY data portal via a AtSAMD21 board, such as an Arduino Zero or Feather M0.

DISCLAIMER:
THIS CODE IS PROVIDED "AS IS" - NO WARRANTY IS GIVEN.
*****************************************************************************/

#define DreamHostPortalRX "xxxx"

// ==========================================================================
//    Include the base required libraries
// ==========================================================================
#include <Arduino.h>  // The base Arduino library
#include <wiring_private.h> // Needed for SAMD pinPeripheral() function
#include <EnableInterrupt.h>  // for external and pin change interrupts


// ==========================================================================
//    Data Logger Settings
// ==========================================================================
// The library version this library was written for
const char *libraryVersion = "0.19.2";
// The name of this file
const char *sketchName = "SAMD_all_sensors.ino";
// Logger ID, also becomes the prefix for the name of the data file on SD card
const char *LoggerID = "XXXXX";
// How frequently (in minutes) to log data
const uint8_t loggingInterval = 1;
// Your logger's timezone.
const int8_t timeZone = -5;  // Eastern Standard Time
// NOTE:  Daylight savings time will not be applied!  Please use standard time!


// ==========================================================================
//    Primary Arduino-Based Board and Processor
// ==========================================================================
#include <sensors/ProcessorStats.h>

const long serialBaud = 115200;   // Baud rate for the primary serial port for debugging
const int8_t greenLED = 8;        // MCU pin for the green LED (-1 if not applicable)
const int8_t redLED = 13;         // MCU pin for the red LED (-1 if not applicable)
const int8_t buttonPin = -1;      // MCU pin for a button to use to enter debugging mode  (-1 if not applicable)
const int8_t wakePin = 6;         // MCU interrupt/alarm pin to wake from sleep
// Set the wake pin to -1 if you do not want the main processor to sleep.
// In a SAMD system where you are using the built-in rtc, set wakePin to 1
const int8_t sdCardPin = 4;       // MCU SD card chip select/slave select pin (must be given!)
const int8_t sensorPowerPin = -1; // MCU pin controlling main sensor power (-1 if not applicable)

// Create and return the processor "sensor"
const char *featherVersion = "M0";
ProcessorStats feather(featherVersion) ;
// Create the battery voltage and free RAM variable objects for the processor and return variable-type pointers to them
// We're going to use the battery variable in the set-up and loop to decide if the battery level is high enough to
// send data over the modem or if the data should only be logged.
// Variable *featherBatt = new ProcessorStats_Batt(&feather, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *featherRAM = new ProcessorStats_FreeRam(&feather, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *featherSampNo = new ProcessorStats_SampleNumber(&feather, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Modem/Internet connection options
// ==========================================================================

// Select your modem chip
// #define TINY_GSM_MODEM_SIM800  // Select for a SIM800, SIM900, or variant thereof
// #define TINY_GSM_MODEM_UBLOX  // Select for most u-blox cellular modems
// #define TINY_GSM_MODEM_ESP8266  // Select for an ESP8266 using the DEFAULT AT COMMAND FIRMWARE
#define TINY_GSM_MODEM_XBEE  // Select for Digi brand WiFi or Cellular XBee's
// #define TINY_GSM_DEBUG Serial
#define TINY_GSM_YIELD() { delay(1); }

// Include TinyGSM for the modem
// This include must be included below the define of the modem name!
#include <TinyGsmClient.h>

 // Set the serial port for the modem - software serial can also be used.
HardwareSerial &ModemSerial = Serial1;

// Create a new TinyGSM modem to run on that serial port and return a pointer to it
TinyGsm *tinyModem = new TinyGsm(ModemSerial);

// Use this to create a modem if you want to spy on modem communication through
// a secondary Arduino stream.  Make sure you install the StreamDebugger library!
// https://github.com/vshymanskyy/StreamDebugger
// #include <StreamDebugger.h>
// StreamDebugger modemDebugger(Serial1, Serial);
// TinyGsm *tinyModem = new TinyGsm(modemDebugger);

// Create a new TCP client on that modem and return a pointer to it
TinyGsmClient *tinyClient = new TinyGsmClient(*tinyModem);

#if defined(TINY_GSM_MODEM_XBEE)
// Describe the physical pin connection of your modem to your board
const long ModemBaud = 9600;        // Communication speed of the modem
const int8_t modemVccPin = -2;      // MCU pin controlling modem power (-1 if not applicable)
const int8_t modemSleepRqPin = 14;  // MCU pin used for modem sleep/wake request (-1 if not applicable)
const int8_t modemStatusPin = 15;   // MCU pin used to read modem status (-1 if not applicable)
const bool modemStatusLevel = LOW;  // The level of the status pin when the module is active (HIGH or LOW)

// And create the wake and sleep methods for the modem
// These can be functions of any type and must return a boolean
// After enabling pin sleep, the sleep request pin is held LOW to keep the XBee on
// Enable pin sleep in the setup function or using XCTU prior to connecting the XBee
bool sleepFxn(void)
{
    if (modemSleepRqPin >= 0)  // Don't go to sleep if there's not a wake pin!
    {
        digitalWrite(modemSleepRqPin, HIGH);
        digitalWrite(redLED, LOW);
        return true;
    }
    else return true;
}
bool wakeFxn(void)
{
    if (modemVccPin >= 0)  // Turns on when power is applied
        return true;
    else if (modemSleepRqPin >= 0)
    {
        digitalWrite(modemSleepRqPin, LOW);
        digitalWrite(redLED, HIGH);  // Because the XBee doesn't have any lights
        return true;
    }
    else return true;
}

#elif defined(TINY_GSM_MODEM_ESP8266)
// Describe the physical pin connection of your modem to your board
const long ModemBaud = 57600;        // Communication speed of the modem
const int8_t modemVccPin = -2;       // MCU pin controlling modem power (-1 if not applicable)
const int8_t modemResetPin = -1;     // MCU pin connected to ESP8266's RSTB pin (-1 if unconnected)
const int8_t espSleepRqPin = 13;     // ESP8266 GPIO pin used for wake from light sleep (-1 if not applicable)
const int8_t modemSleepRqPin = 15;   // MCU pin used for wake from light sleep (-1 if not applicable)
const int8_t espStatusPin = -1;      // ESP8266 GPIO pin used to give modem status (-1 if not applicable)
const int8_t modemStatusPin = -1;    // MCU pin used to read modem status (-1 if not applicable)
const bool modemStatusLevel = HIGH;  // The level of the status pin when the module is active (HIGH or LOW)

// And create the wake and sleep methods for the modem
// These can be functions of any type and must return a boolean
bool sleepFxn(void)
{
    // Use this if you have an MCU pin connected to the ESP's reset pin to wake from deep sleep
    if (modemResetPin >= 0)
    {
        digitalWrite(redLED, LOW);
        return tinyModem->poweroff();
    }
    // Use this if you have GPIO16 connected to the reset pin to wake from deep sleep
    // but no other MCU pin connected to the reset pin.
    // NOTE:  This will NOT work nicely with things like "testingMode" and the
    // initial 2-minute logging interval at boot up.
    // if (loggingInterval > 1)
    // {
    //     uint32_t sleepSeconds = (((uint32_t)loggingInterval) * 60 * 1000) - 75000L;
    //     String sleepCommand = String(sleepSeconds);
    //     tinyModem->sendAT(F("+GSLP="), sleepCommand);
    //     // Power down for 1 minute less than logging interval
    //     // Better:  Calculate length of loop and power down for logging interval - loop time
    //     return tinyModem->waitResponse() == 1;
    // }
    // Use this if you don't have access to the ESP8266's reset pin for deep sleep but you
    // do have access to another GPIO pin for light sleep.  This also sets up another
    // pin to view the sleep status.
    else if (modemSleepRqPin >= 0 && modemStatusPin >= 0)
    {
        tinyModem->sendAT(F("+WAKEUPGPIO=1,"), String(espSleepRqPin), F(",0,"),
                          String(espStatusPin), F(","), modemStatusLevel);
        bool success = tinyModem->waitResponse() == 1;
        tinyModem->sendAT(F("+SLEEP=1"));
        success &= tinyModem->waitResponse() == 1;
        digitalWrite(redLED, LOW);
        return success;
    }
    // Light sleep without the status pin
    else if (modemSleepRqPin >= 0 && modemStatusPin < 0)
    {
        tinyModem->sendAT(F("+WAKEUPGPIO=1,"), String(espSleepRqPin), F(",0"));
        bool success = tinyModem->waitResponse() == 1;
        tinyModem->sendAT(F("+SLEEP=1"));
        success &= tinyModem->waitResponse() == 1;
        digitalWrite(redLED, LOW);
        return success;
    }
    else return true;  // DON'T go to sleep if we can't wake up!
}
bool wakeFxn(void)
{
    if (modemVccPin >= 0)  // Turns on when power is applied
    {
        digitalWrite(redLED, HIGH);  // Because the ESP8266 doesn't have any lights
        return true;
    }
    else if (modemResetPin >= 0)
    {
        digitalWrite(modemResetPin, LOW);
        delay(1);
        digitalWrite(modemResetPin, HIGH);
        digitalWrite(redLED, HIGH);
        return true;
    }
    else if (modemSleepRqPin >= 0)
    {
        digitalWrite(modemSleepRqPin, LOW);
        delay(1);
        digitalWrite(modemSleepRqPin, HIGH);
        digitalWrite(redLED, HIGH);
        return true;
    }
    else return true;
}

#elif defined(TINY_GSM_MODEM_UBLOX)
// Describe the physical pin connection of your modem to your board
const long ModemBaud = 9600;         // Communication speed of the modem
const int8_t modemVccPin = 9;        // MCU pin controlling modem power (-1 if not applicable)
const int8_t modemSleepRqPin = 14;   // MCU pin used for modem sleep/wake request (-1 if not applicable)
const int8_t modemStatusPin = 15;    // MCU pin used to read modem status (-1 if not applicable)
const bool modemStatusLevel = HIGH;  // The level of the status pin when the module is active (HIGH or LOW)

// And create the wake and sleep methods for the modem
// These can be functions of any type and must return a boolean
bool sleepFxn(void)
{
    if (modemVccPin >= 0 && modemSleepRqPin < 0)
        return tinyModem->poweroff();
    else if (modemSleepRqPin >= 0)
    {
        digitalWrite(modemSleepRqPin, LOW);
        digitalWrite(redLED, HIGH);  // A light to watch to verify pulse timing
        delay(1100);  // >1s pulse for power down
        digitalWrite(modemSleepRqPin, HIGH);
        digitalWrite(redLED, LOW);
        return true;
    }
    else return true;  // DON'T go to sleep if we can't wake up!
}
bool wakeFxn(void)
{
    if (modemVccPin >= 0)  // Turns on when power is applied
        return true;
    else if(modemSleepRqPin >= 0)
    {
        digitalWrite(modemSleepRqPin, LOW);
        digitalWrite(redLED, HIGH);
        delay(200); // 0.15-3.2s pulse for wake on SARA R4/N4
        // delayMicroseconds(65); // 50-80µs pulse for wake on SARA/LISA U2/G2
        digitalWrite(modemSleepRqPin, HIGH);
        digitalWrite(redLED, LOW);
        return true;
    }
    else return true;
}

#else
// Describe the physical pin connection of your modem to your board
const long ModemBaud = 9600;         // Communication speed of the modem
const int8_t modemVccPin = -2;       // MCU pin controlling modem power (-1 if not applicable)
const int8_t modemSleepRqPin = 14;   // MCU pin used for modem sleep/wake request (-1 if not applicable)
const int8_t modemStatusPin = 15;    // MCU pin used to read modem status (-1 if not applicable)
const bool modemStatusLevel = HIGH;  // The level of the status pin when the module is active (HIGH or LOW)

// And create the wake and sleep methods for the modem
// These can be functions of any type and must return a boolean
bool wakeFxn(void)
{
    digitalWrite(modemSleepRqPin, HIGH);
    digitalWrite(redLED, HIGH);  // A light just for show
    return true;
}
bool sleepFxn(void)
{
    digitalWrite(modemSleepRqPin, LOW);
    digitalWrite(redLED, LOW);
    return true;
}
#endif

// And we still need the connection information for the network
const char *apn = "xxxxx";  // The APN for the gprs connection, unnecessary for WiFi
const char *wifiId = "xxxxx";  // The WiFi access point, unnecessary for gprs
const char *wifiPwd = "xxxxx";  // The password for connecting to WiFi, unnecessary for gprs

// Create the loggerModem instance
#include <LoggerModem.h>
// A "loggerModem" is a combination of a TinyGSM Modem, a Client, and functions for wake and sleep
#if defined(TINY_GSM_MODEM_ESP8266)
loggerModem modem(modemVccPin, modemStatusPin, modemStatusLevel, wakeFxn, sleepFxn, tinyModem, tinyClient, wifiId, wifiPwd);
#elif defined(TINY_GSM_MODEM_XBEE)
loggerModem modem(modemVccPin, modemStatusPin, modemStatusLevel, wakeFxn, sleepFxn, tinyModem, tinyClient, wifiId, wifiPwd);
// loggerModem modem(modemVccPin, modemStatusPin, modemStatusLevel, wakeFxn, sleepFxn, tinyModem, tinyClient, apn);
#elif defined(TINY_GSM_MODEM_UBLOX)
loggerModem modem(modemVccPin, modemStatusPin, modemStatusLevel, wakeFxn, sleepFxn, tinyModem, tinyClient, apn);
#else
loggerModem modem(modemVccPin, modemStatusPin, modemStatusLevel, wakeFxn, sleepFxn, tinyModem, tinyClient, apn);
#endif

// Create the RSSI and signal strength variable objects for the modem and return
// variable-type pointers to them
// Variable *modemRSSI = new Modem_RSSI(&modem, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *modemSignalPct = new Modem_SignalPercent(&modem, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Maxim DS3231 RTC (Real Time Clock)
// ==========================================================================
#include <sensors/MaximDS3231.h>
// Create and return the DS3231 sensor object
MaximDS3231 ds3231(1);
// Create the temperature variable object for the DS3231 and return a variable-type pointer to it
// Variable *ds3231Temp = new MaximDS3231_Temp(&ds3231, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    AOSong AM2315 Digital Humidity and Temperature Sensor
// ==========================================================================
#include <sensors/AOSongAM2315.h>
const int8_t I2CPower = -1;  // Pin to switch power on and off (-1 if unconnected)
// Create and return the AOSong AM2315 sensor object
AOSongAM2315 am2315(I2CPower);
// Create the humidity and temperature variable objects for the AM2315 and return variable-type pointers to them
// Variable *am2315Humid = new AOSongAM2315_Humidity(&am2315, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *am2315Temp = new AOSongAM2315_Temp(&am2315, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    AOSong DHT 11/21 (AM2301)/22 (AM2302) Digital Humidity and Temperature
// ==========================================================================
#include <sensors/AOSongDHT.h>
const int8_t DHTPower = -1;  // Pin to switch power on and off (-1 if unconnected)
const int8_t DHTPin = 16;  // DHT data pin
DHTtype dhtType = DHT22;  // DHT type, either DHT11, DHT21, or DHT22
// Create and return the AOSong DHT sensor object
AOSongDHT dht(DHTPower, DHTPin, dhtType);
// Create the humidity, temperature and heat index variable objects for the DHT
// and return variable-type pointers to them
// Variable *dhtHumid = new AOSongDHT_Humidity(&dht, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *dhtTemp = new AOSongDHT_Temp(&dht, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *dhtHI = new AOSongDHT_HI(&dht, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Apogee SQ-212 Photosynthetically Active Radiation (PAR) Sensor
// ==========================================================================
#include <sensors/ApogeeSQ212.h>
const int8_t SQ212Power = -1;  // Pin to switch power on and off (-1 if unconnected)
const int8_t SQ212Data = 2;  // The data pin ON THE ADS1115 (NOT the Arduino Pin Number)
const uint8_t SQ212_ADS1115Address = 0x48;  // The I2C address of the ADS1115 ADC
// Create and return the Apogee SQ212 sensor object
ApogeeSQ212 SQ212(SQ212Power, SQ212Data);
// Create the PAR variable object for the SQ212 and return a variable-type pointer to it
// Variable *SQ212PAR = new ApogeeSQ212_PAR(&SQ212, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Bosch BME280 Environmental Sensor (Temperature, Humidity, Pressure)
// ==========================================================================
#include <sensors/BoschBME280.h>
uint8_t BMEi2c_addr = 0x76;
// The BME280 can be addressed either as 0x77 (Adafruit default) or 0x76 (Grove default)
// Either can be physically mofidied for the other address
// const int8_t I2CPower = -1;  // Pin to switch power on and off (-1 if unconnected)
// Create and return the Bosch BME280 sensor object
BoschBME280 bme280(I2CPower, BMEi2c_addr);
// Create the four variable objects for the BME280 and return variable-type pointers to them
// Variable *bme280Humid = new BoschBME280_Humidity(&bme280, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *bme280Temp = new BoschBME280_Temp(&bme280, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *bme280Press = new BoschBME280_Pressure(&bme280, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *bme280Alt = new BoschBME280_Altitude(&bme280, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    CAMPBELL OBS 3 / OBS 3+ Analog Turbidity Sensor
// ==========================================================================
#include <sensors/CampbellOBS3.h>
const int8_t OBS3Power = -1;  // Pin to switch power on and off (-1 if unconnected)
const uint8_t OBS3numberReadings = 1;
const uint8_t OBS3_ADS1115Address = 0x48;  // The I2C address of the ADS1115 ADC
// Campbell OBS 3+ Low Range calibration in Volts
const int8_t OBSLowPin = 2;  // The low voltage analog pin ON THE ADS1115 (NOT the Arduino Pin Number)
const float OBSLow_A = 4.0749E+00;  // The "A" value (X^2) from the low range calibration
const float OBSLow_B = 9.1011E+01;  // The "B" value (X) from the low range calibration
const float OBSLow_C = -3.9570E-01;  // The "C" value from the low range calibration
// Create and return the Campbell OBS3+ LOW RANGE sensor object
CampbellOBS3 osb3low(OBS3Power, OBSLowPin, OBSLow_A, OBSLow_B, OBSLow_C, OBS3_ADS1115Address, OBS3numberReadings);
// Create the turbidity and voltage variable objects for the low range OBS3 and return variable-type pointers to them
// Variable *obs3TurbLow = new CampbellOBS3_Turbidity(&osb3low, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *obs3VoltLow = new CampbellOBS3_Voltage(&osb3low, "12345678-abcd-1234-efgh-1234567890ab");

// Campbell OBS 3+ High Range calibration in Volts
const int8_t OBSHighPin = 3;  // The high voltage analog pin ON THE ADS1115 (NOT the Arduino Pin Number)
const float OBSHigh_A = 5.2996E+01;  // The "A" value (X^2) from the high range calibration
const float OBSHigh_B = 3.7828E+02;  // The "B" value (X) from the high range calibration
const float OBSHigh_C = -1.3927E+00;  // The "C" value from the high range calibration
// Create and return the Campbell OBS3+ HIGH RANGE sensor object
CampbellOBS3 osb3high(OBS3Power, OBSHighPin, OBSHigh_A, OBSHigh_B, OBSHigh_C, OBS3_ADS1115Address, OBS3numberReadings);
// Create the turbidity and voltage variable objects for the high range OBS3 and return variable-type pointers to them
// Variable *obs3TurbHigh = new CampbellOBS3_Turbidity(&osb3high, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *obs3VoltHigh = new CampbellOBS3_Voltage(&osb3high, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Decagon 5TM Soil Moisture Sensor
// ==========================================================================
#include <sensors/Decagon5TM.h>
const char *TMSDI12address = "2";  // The SDI-12 Address of the 5-TM
const int8_t SDI12Data = 17;  // The pin the 5TM is attached to
const int8_t SDI12Power = -1;  // Pin to switch power on and off (-1 if unconnected)
// Create and return the Decagon 5TM sensor object
Decagon5TM fivetm(*TMSDI12address, SDI12Power, SDI12Data);
// Decagon5TM fivetm(*TMSDI12address, SDI12Bus, SDI12Power);
// Create the matric potential, volumetric water content, and temperature
// variable objects for the 5TM and return variable-type pointers to them
// Variable *fivetmEa = new Decagon5TM_Ea(&fivetm, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *fivetmVWC = new Decagon5TM_VWC(&fivetm, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *fivetmTemp = new Decagon5TM_Temp(&fivetm, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Decagon CTD Conductivity, Temperature, and Depth Sensor
// ==========================================================================
#include <sensors/DecagonCTD.h>
const char *CTDSDI12address = "1";  // The SDI-12 Address of the CTD
const uint8_t CTDnumberReadings = 6;  // The number of readings to average
// const int8_t SDI12Data = 17;  // The pin the CTD is attached to
// const int8_t SDI12Power = -1;  // Pin to switch power on and off (-1 if unconnected)
// Create and return the Decagon CTD sensor object
DecagonCTD ctd(*CTDSDI12address, SDI12Power, SDI12Data, CTDnumberReadings);
// DecagonCTD ctd(*CTDSDI12address, SDI12Bus, SDI12Power, CTDnumberReadings);
// Create the conductivity, temperature, and depth variable objects for the CTD
// and return variable-type pointers to them
// Variable *ctdCond = new DecagonCTD_Cond(&ctd, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *ctdTemp = new DecagonCTD_Temp(&ctd, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *ctdDepth = new DecagonCTD_Depth(&ctd, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Decagon ES2 Conductivity and Temperature Sensor
// ==========================================================================
#include <sensors/DecagonES2.h>
const char *ES2SDI12address = "3";  // The SDI-12 Address of the ES2
// const int8_t SDI12Data = 17;  // The pin the ES2 is attached to
// const int8_t SDI12Power = -1;  // Pin to switch power on and off (-1 if unconnected)
const uint8_t ES2NumberReadings = 3;
// Create and return the Decagon ES2 sensor object
DecagonES2 es2(*ES2SDI12address, SDI12Power, SDI12Data, ES2NumberReadings);
// DecagonES2 es2(*ES2SDI12address, SDI12Bus, SDI12Power, ES2NumberReadings);
// Create the conductivity and temperature variable objects for the ES2 and return variable-type pointers to them
// Variable *es2Cond = new DecagonES2_Cond(&es2, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *es2Temp = new DecagonES2_Temp(&es2, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    External Voltage via TI ADS1115
// ==========================================================================
#include <sensors/ExternalVoltage.h>
const int8_t VoltPower = -1;  // Pin to switch power on and off (-1 if unconnected)
const int8_t VoltData = 2;  // The data pin ON THE ADS1115 (NOT the Arduino Pin Number)
const float VoltGain = 10; // Default 1/gain for grove voltage divider is 10x
const uint8_t Volt_ADS1115Address = 0x48;  // The I2C address of the ADS1115 ADC
const uint8_t VoltReadsToAvg = 1; // Only read one sample
// Create and return the External Voltage sensor object
ExternalVoltage extvolt(VoltPower, VoltData, VoltGain, Volt_ADS1115Address, VoltReadsToAvg);
// Create the voltage variable object and return a variable-type pointer to it
// Variable *extvoltV = new ExternalVoltage_Volt(&extvolt, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Freescale Semiconductor MPL115A2 Barometer
// ==========================================================================
#include <sensors/FreescaleMPL115A2.h>
// const int8_t I2CPower = -1;  // Pin to switch power on and off (-1 if unconnected)
const uint8_t MPL115A2ReadingsToAvg = 1;
// Create and return the MPL115A2 barometer sensor object
MPL115A2 mpl115a2(I2CPower, MPL115A2ReadingsToAvg);
// Create the pressure and temperature variable objects for the MPL and return variable-type pointer to them
// Variable *mplPress = new MPL115A2_Pressure(&mpl115a2, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *mplTemp = new MPL115A2_Temp(&mpl115a2, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Maxbotix HRXL Ultrasonic Range Finder
// ==========================================================================

// Set up a 'new' UART for receiving sonar data - in this case, using SERCOM2
// In this case, the Rx will be on digital pin 5, which is SERCOM2's Pad #3
// Although the MaxBotix cannot use it, the Tx will be on digital pin 2, which is SERCOM2's Pad #2
// NOTE:  SERCOM2 is undefinied on a "standard" Arduino Zero and many clones,
//        but not all!  Please check the variant.cpp file for you individual board!
//        Sodaq Autonomo's and Sodaq One's do NOT follow the 'standard' SERCOM definitions!
Uart Serial3(&sercom2, 5, 2, SERCOM_RX_PAD_3, UART_TX_PAD_2);
// Hand over the interrupts to the sercom port
void SERCOM2_Handler()
{
    Serial3.IrqHandler();
}
// This is just a pointer to help me remember the port name
HardwareSerial &sonarSerial = Serial3;

#include <sensors/MaxBotixSonar.h>
const int8_t SonarPower = -1;  // Excite (power) pin (-1 if unconnected)
const int8_t Sonar1Trigger = 18;  // Trigger pin (a negative number if unconnected)
const int8_t Sonar2Trigger = 19;  // Trigger pin (a negative number if unconnected)
// Create and return the MaxBotix Sonar sensor object
MaxBotixSonar sonar1(sonarSerial, SonarPower, Sonar1Trigger) ;
// Create the voltage variable object and return a variable-type pointer to it
// Variable *sonar1Range = new MaxBotixSonar_Range(&sonar1, "12345678-abcd-1234-efgh-1234567890ab");

MaxBotixSonar sonar2(sonarSerial, SonarPower, Sonar2Trigger) ;


// ==========================================================================
//    Maxim DS18 One Wire Temperature Sensor
// ==========================================================================
#include <sensors/MaximDS18.h>
// OneWire Address [array of 8 hex characters]
DeviceAddress OneWireAddress1 = {0x28, 0xFF, 0xBD, 0xBA, 0x81, 0x16, 0x03, 0x0C};
DeviceAddress OneWireAddress2 = {0x28, 0xFF, 0x57, 0x90, 0x82, 0x16, 0x04, 0x67};
DeviceAddress OneWireAddress3 = {0x28, 0xFF, 0x74, 0x2B, 0x82, 0x16, 0x03, 0x57};
DeviceAddress OneWireAddress4 = {0x28, 0xFF, 0xB6, 0x6E, 0x84, 0x16, 0x05, 0x9B};
DeviceAddress OneWireAddress5 = {0x28, 0xFF, 0x3B, 0x07, 0x82, 0x16, 0x03, 0xB3};
const int8_t OneWireBus = 12;  // Pin attached to the OneWire Bus (-1 if unconnected)
const int8_t OneWirePower = -1;  // Pin to switch power on and off (-1 if unconnected)
// Create and return the Maxim DS18 sensor objects (use this form for a known address)
MaximDS18 ds18_1(OneWireAddress1, OneWirePower, OneWireBus);
MaximDS18 ds18_2(OneWireAddress2, OneWirePower, OneWireBus);
MaximDS18 ds18_3(OneWireAddress3, OneWirePower, OneWireBus);
MaximDS18 ds18_4(OneWireAddress4, OneWirePower, OneWireBus);
MaximDS18 ds18_5(OneWireAddress5, OneWirePower, OneWireBus);
// Create and return the Maxim DS18 sensor object (use this form for a single sensor on bus with an unknown address)
// MaximDS18 ds18_u(OneWirePower, OneWireBus);
// Create the temperature variable object for the DS18 and return a variable-type pointer to it
// Variable *ds18Temp = new MaximDS18_Temp(&ds18_u, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    MeaSpecMS5803 (Pressure, Temperature)
// ==========================================================================
#include <sensors/MeaSpecMS5803.h>
// const int8_t I2CPower = 22;  // Pin to switch power on and off (-1 if unconnected)
const uint8_t MS5803i2c_addr = 0x76;  // The MS5803 can be addressed either as 0x76 or 0x77
const int16_t MS5803maxPressure = 14;  // The maximum pressure measurable by the specific MS5803 model
const uint8_t MS5803ReadingsToAvg = 1;
// Create and return the MeaSpec MS5803 pressure and temperature sensor object
MeaSpecMS5803 ms5803(I2CPower, MS5803i2c_addr, MS5803maxPressure, MS5803ReadingsToAvg);
// Create the conductivity and temperature variable objects for the ES2 and return variable-type pointers to them
// Variable *ms5803Press = new MeaSpecMS5803_Pressure(&ms5803, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *ms5803Temp = new MeaSpecMS5803_Temp(&ms5803, "12345678-abcd-1234-efgh-1234567890ab");


/***
// ==========================================================================
//    PaleoTerraRedox (Oxidation-reduction potential)
// ==========================================================================
#include <sensors/PaleoTerraRedox.h>
// const int8_t I2CPower = 22;  // Pin to switch power on and off (-1 if unconnected)
const int8_t sclPin1 = 4;  //Clock pin to be used with 1st redox probe
const int8_t sdaPin1 = 5;  //Data pin to be used with 1st redox probe
const int8_t sclPin2 = 6;  //Clock pin to be used with 2nd redox probe
const int8_t sdaPin2 = 7;  //Data pin to be used with 2nd redox probe
const int8_t sclPin3 = 10;  //Clock pin to be used with 2nd redox probe
const int8_t sdaPin3 = 11;  //Data pin to be used with 2nd redox probe
const int8_t PaleoTerraReadingsToAvg = 1;
Create and return the Paleo Terra Redox sensor objects
PaleoTerraRedox redox1(I2CPower, sclPin1, sdaPin1, PaleoTerraReadingsToAvg);
PaleoTerraRedox redox2(I2CPower, sclPin2, sdaPin2, PaleoTerraReadingsToAvg);
PaleoTerraRedox redox3(I2CPower, sclPin3, sdaPin3, PaleoTerraReadingsToAvg);
***/


// ==========================================================================
//    External I2C Rain Tipping Bucket Counter
// ==========================================================================
#include <sensors/RainCounterI2C.h>
const uint8_t RainCounterI2CAddress = 0x08;  // I2C Address for external tip counter
const float depthPerTipEvent = 0.2;  // rain depth in mm per tip event
// Create and return the Rain Counter sensor object
RainCounterI2C tbi2c(RainCounterI2CAddress, depthPerTipEvent);
// Create the conductivity and temperature variable objects for the ES2 and return variable-type pointers to them
// Variable *tbi2cTips = new RainCounterI2C_Tips(&tbi2c, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *tbi2cDepth = new RainCounterI2C_Depth(&tbi2c, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    TI INA219 High Side Current/Voltage Sensor (Current mA, Voltage, Power)
// ==========================================================================
#include <sensors/TIINA219.h>
uint8_t INA219i2c_addr = 0x40; // 1000000 (Board A0+A1=GND)
// The INA219 can be addressed either as 0x40 (Adafruit default) or 0x41 44 45
// Either can be physically mofidied for the other address
// const int8_t I2CPower = 22;  // Pin to switch power on and off (-1 if unconnected)
const uint8_t INA219ReadingsToAvg = 1;
// Create and return the INA219 sensor object
TIINA219 ina219(I2CPower, INA219i2c_addr, INA219ReadingsToAvg);
// Create the current, voltage, and power variable objects for the Nanolevel and return variable-type pointers to them
// Variable *inaCurrent = new TIINA219_Current(&ina219, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *inaVolt = new TIINA219_Volt(&ina219, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *inaPower = new TIINA219_Power(&ina219, "12345678-abcd-1234-efgh-1234567890ab");


// Set up a 'new' UART for modbus communication - in this case, using SERCOM1
// In this case, the Rx will be on digital pin 11, which is SERCOM1's Pad #0
// The Tx will be on digital pin 10, which is SERCOM1's Pad #2
// NOTE:  SERCOM1 is undefinied on a "standard" Arduino Zero and many clones,
//        but not all!  Please check the variant.cpp file for you individual board!
//        Sodaq Autonomo's and Sodaq One's do NOT follow the 'standard' SERCOM definitions!
Uart Serial2(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);
// Hand over the interrupts to the sercom port
void SERCOM1_Handler()
{
    Serial2.IrqHandler();
}
// This is just a pointer to help me remember the port name
HardwareSerial &modbusSerial = Serial2;

// ==========================================================================
//    Keller Acculevel High Accuracy Submersible Level Transmitter
// ==========================================================================
#include <sensors/KellerAcculevel.h>
byte acculevelModbusAddress = 0x01;  // The modbus address of KellerAcculevel
const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t acculevelNumberReadings = 5;  // The manufacturer recommends taking and averaging a few readings
// Create and return the Keller Acculevel sensor object
KellerAcculevel acculevel(acculevelModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, acculevelNumberReadings);
// Create the pressure, temperature, and height variable objects for the Acculevel and return variable-type pointers to them
// Variable *acculevPress = new KellerAcculevel_Pressure(&acculevel, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *acculevTemp = new KellerAcculevel_Temp(&acculevel, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *acculevHeight = new KellerAcculevel_Height(&acculevel, "12345678-abcd-1234-efgh-1234567890ab");

// ==========================================================================
//    Keller Nanolevel High Accuracy Submersible Level Transmitter
// ==========================================================================
#include <sensors/KellerNanolevel.h>
byte nanolevelModbusAddress = 0x01;  // The modbus address of KellerNanolevel
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t nanolevelNumberReadings = 3;  // The manufacturer recommends taking and averaging a few readings
// Create and return the Keller Nanolevel sensor object
KellerNanolevel nanolevel(nanolevelModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, nanolevelNumberReadings);
// Create the pressure, temperature, and height variable objects for the Nanolevel and return variable-type pointers to them
// Variable *acculevPress = new KellerNanolevel_Pressure(&nanolevel, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *acculevTemp = new KellerNanolevel_Temp(&nanolevel, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *acculevHeight = new KellerNanolevel_Height(&nanolevel, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y504 Dissolved Oxygen Sensor
// ==========================================================================
#include <sensors/YosemitechY504.h>
byte y504ModbusAddress = 0x04;  // The modbus address of the Y504
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y504NumberReadings = 5;  // The manufacturer recommends averaging 10 readings, but we take 5 to minimize power consumption
// Create and return the Yosemitech Y504 dissolved oxygen sensor object
YosemitechY504 y504(y504ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y504NumberReadings);
// Create the dissolved oxygen percent, dissolved oxygen concentration, and
// temperature variable objects for the Y504 and return variable-type
// pointers to them
// Variable *y504DOpct = new YosemitechY504_DOpct(&y504, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y504DOmgL = new YosemitechY504_DOmgL(&y504, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y504Temp = new YosemitechY504_Temp(&y504, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y510 Turbidity Sensor
// ==========================================================================
#include <sensors/YosemitechY510.h>
byte y510ModbusAddress = 0x0B;  // The modbus address of the Y510
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y510NumberReadings = 5;  // The manufacturer recommends averaging 10 readings, but we take 5 to minimize power consumption
// Create and return the Y510-B Turbidity sensor object
YosemitechY510 y510(y510ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y510NumberReadings);
// Create the turbidity and temperature variable objects for the Y510 and return variable-type pointers to them
// Variable *y510Turb = new YosemitechY510_Turbidity(&y510, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y510Temp = new YosemitechY510_Temp(&y510, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y511 Turbidity Sensor with Wiper
// ==========================================================================
#include <sensors/YosemitechY511.h>
byte y511ModbusAddress = 0x1A;  // The modbus address of the Y511
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y511NumberReadings = 5;  // The manufacturer recommends averaging 10 readings, but we take 5 to minimize power consumption
// Create and return the Y511-A Turbidity sensor object
YosemitechY511 y511(y511ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y511NumberReadings);
// Create the turbidity and temperature variable objects for the Y511 and return variable-type pointers to them
// Variable *y511Turb = new YosemitechY511_Turbidity(&y511, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y511Temp = new YosemitechY511_Temp(&y511, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y514 Chlorophyll Sensor
// ==========================================================================
#include <sensors/YosemitechY514.h>
byte y514ModbusAddress = 0x14;  // The modbus address of the Y514
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y514NumberReadings = 5;  // The manufacturer recommends averaging 10 readings, but we take 5 to minimize power consumption
// Create and return the Y514 chlorophyll sensor object
YosemitechY514 y514(y514ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y514NumberReadings);
// Create the chlorophyll concentration and temperature variable objects for the Y514 and return variable-type pointers to them
// Variable *y514Chloro = new YosemitechY514_Chlorophyll(&y514, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y514Temp = new YosemitechY514_Temp(&y514, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y520 Conductivity Sensor
// ==========================================================================
#include <sensors/YosemitechY520.h>
byte y520ModbusAddress = 0x20;  // The modbus address of the Y520
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y520NumberReadings = 5;  // The manufacturer recommends averaging 10 readings, but we take 5 to minimize power consumption
// Create and return the Y520 conductivity sensor object
YosemitechY520 y520(y520ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y520NumberReadings);
// Create the specific conductance and temperature variable objects for the Y520 and return variable-type pointers to them
// Variable *y520Cond = new YosemitechY520_Cond(&y520, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y520Temp = new YosemitechY520_Temp(&y520, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y532 pH
// ==========================================================================
#include <sensors/YosemitechY532.h>
byte y532ModbusAddress = 0x32;  // The modbus address of the Y532
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y532NumberReadings = 1;  // The manufacturer actually doesn't mention averaging for this one
// Create and return the Yosemitech Y532 pH sensor object
YosemitechY532 y532(y532ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y532NumberReadings);
// Create the pH, electrical potential, and temperature variable objects for the Y532 and return variable-type pointers to them
// Variable *y532Voltage = new YosemitechY532_Voltage(&y532, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y532pH = new YosemitechY532_pH(&y532, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y532Temp = new YosemitechY532_Temp(&y532, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y550 COD Sensor with Wiper
// ==========================================================================
#include <sensors/YosemitechY550.h>
byte y550ModbusAddress = 0x50;  // The modbus address of the Y550
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y550NumberReadings = 5;  // The manufacturer recommends averaging 10 readings, but we take 5 to minimize power consumption
// Create and return the Y550 conductivity sensor object
YosemitechY550 y550(y550ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y550NumberReadings);
// Create the COD, turbidity, and temperature variable objects for the Y550 and return variable-type pointers to them
// Variable *y550COD = new YosemitechY550_COD(&y550, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y550Turbid = new YosemitechY550_Turbidity(&y550, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y550Temp = new YosemitechY550_Temp(&y550, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Yosemitech Y4000 Multiparameter Sonde (DOmgL, Turbidity, Cond, pH, Temp, ORP, Chlorophyll, BGA)
// ==========================================================================
#include <sensors/YosemitechY4000.h>
byte y4000ModbusAddress = 0x05;  // The modbus address of the Y4000
// const int8_t rs485AdapterPower = -1;  // Pin to switch RS485 adapter power on and off (-1 if unconnected)
// const int8_t modbusSensorPower = -1;  // Pin to switch sensor power on and off (-1 if unconnected)
// const int8_t max485EnablePin = -1;  // Pin connected to the RE/DE on the 485 chip (-1 if unconnected)
const uint8_t y4000NumberReadings = 5;  // The manufacturer recommends averaging 10 readings, but we take 5 to minimize power consumption
// Create and return the Yosemitech Y4000 multi-parameter sensor object
YosemitechY4000 y4000(y4000ModbusAddress, modbusSerial, rs485AdapterPower, modbusSensorPower, max485EnablePin, y4000NumberReadings);
// Create all of the variable objects for the Y4000 and return variable-type pointers to them
// Variable *y4000DO = new YosemitechY4000_DOmgL(&y4000, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y4000Turb = new YosemitechY4000_Turbidity(&y4000, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y4000Cond = new YosemitechY4000_Cond(&y4000, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y4000pH = new YosemitechY4000_pH(&y4000, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y4000Temp = new YosemitechY4000_Temp(&y4000, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y4000ORP = new YosemitechY4000_ORP(&y4000, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y4000Chloro = new YosemitechY4000_Chlorophyll(&y4000, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *y4000BGA = new YosemitechY4000_BGA(&y4000, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    Zebra Tech D-Opto Dissolved Oxygen Sensor
// ==========================================================================
#include <sensors/ZebraTechDOpto.h>
const char *DOptoDI12address = "5";  // The SDI-12 Address of the Zebra Tech D-Opto
// const int8_t SDI12Data = 7;  // The pin the D-Opto is attached to
// const int8_t SDI12Power = 22;  // Pin to switch power on and off (-1 if unconnected)
// Create and return the Zebra Tech DOpto dissolved oxygen sensor object
ZebraTechDOpto dopto(*DOptoDI12address, SDI12Power, SDI12Data);
// ZebraTechDOpto dopto(*DOptoDI12address, SDI12Bus, SDI12Power);
// Create the dissolved oxygen percent, dissolved oxygen concentration, and
// temperature variable objects for the Zebra Tech and return variable-type
// pointers to them
// Variable *dOptoDOpct = new ZebraTechDOpto_DOpct(&dopto, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *dOptoDOmgL = new ZebraTechDOpto_DOmgL(&dopto, "12345678-abcd-1234-efgh-1234567890ab");
// Variable *dOptoTemp = new ZebraTechDOpto_Temp(&dopto, "12345678-abcd-1234-efgh-1234567890ab");


// ==========================================================================
//    The array that contains all variables to be logged
// ==========================================================================
#include <VariableArray.h>
// Create pointers for all of the variables from the sensors
// at the same time putting them into an array
Variable *variableList[] = {
    new ProcessorStats_SampleNumber(&feather, "12345678-abcd-1234-efgh-1234567890ab"),
    new ApogeeSQ212_PAR(&SQ212, "00200000-abcd-1234-efgh-1234567890ab"),
    new AOSongAM2315_Humidity(&am2315, "00300000-abcd-1234-efgh-1234567890ab"),
    new AOSongAM2315_Temp(&am2315, "00400000-abcd-1234-efgh-1234567890ab"),
    new AOSongDHT_Humidity(&dht, "00500000-abcd-1234-efgh-1234567890ab"),
    new AOSongDHT_Temp(&dht, "00600000-abcd-1234-efgh-1234567890ab"),
    new AOSongDHT_HI(&dht, "00700000-abcd-1234-efgh-1234567890ab"),
    new BoschBME280_Temp(&bme280, "00800000-abcd-1234-efgh-1234567890ab"),
    new BoschBME280_Humidity(&bme280, "00900000-abcd-1234-efgh-1234567890ab"),
    new BoschBME280_Pressure(&bme280, "01000000-abcd-1234-efgh-1234567890ab"),
    new BoschBME280_Altitude(&bme280, "01100000-abcd-1234-efgh-1234567890ab"),
    new CampbellOBS3_Turbidity(&osb3low, "01200000-abcd-1234-efgh-1234567890ab", "TurbLow"),
    new CampbellOBS3_Voltage(&osb3low, "01300000-abcd-1234-efgh-1234567890ab", "TurbLowV"),
    new CampbellOBS3_Turbidity(&osb3high, "01400000-abcd-1234-efgh-1234567890ab", "TurbHigh"),
    new CampbellOBS3_Voltage(&osb3high, "01500000-abcd-1234-efgh-1234567890ab", "TurbHighV"),
    new Decagon5TM_Ea(&fivetm, "01600000-abcd-1234-efgh-1234567890ab"),
    new Decagon5TM_Temp(&fivetm, "01700000-abcd-1234-efgh-1234567890ab"),
    new Decagon5TM_VWC(&fivetm, "01800000-abcd-1234-efgh-1234567890ab"),
    new DecagonCTD_Cond(&ctd, "01900000-abcd-1234-efgh-1234567890ab"),
    new DecagonCTD_Temp(&ctd, "02000000-abcd-1234-efgh-1234567890ab"),
    new DecagonCTD_Depth(&ctd, "02100000-abcd-1234-efgh-1234567890ab"),
    new DecagonES2_Cond(&es2, "02500000-abcd-1234-efgh-1234567890ab"),
    new DecagonES2_Temp(&es2, "02600000-abcd-1234-efgh-1234567890ab"),
    new ExternalVoltage_Volt(&extvolt, "02700000-abcd-1234-efgh-1234567890ab"),
    new MaxBotixSonar_Range(&sonar1, "02800000-abcd-1234-efgh-1234567890ab"),
    new MaxBotixSonar_Range(&sonar2, "02900000-abcd-1234-efgh-1234567890ab"),
    new MaximDS18_Temp(&ds18_1, "03000000-abcd-1234-efgh-1234567890ab"),
    new MaximDS18_Temp(&ds18_2, "03100000-abcd-1234-efgh-1234567890ab"),
    new MaximDS18_Temp(&ds18_3, "03200000-abcd-1234-efgh-1234567890ab"),
    new MaximDS18_Temp(&ds18_4, "03300000-abcd-1234-efgh-1234567890ab"),
    new MaximDS18_Temp(&ds18_5, "03400000-abcd-1234-efgh-1234567890ab"),
    // new MaximDS18_Temp(&ds18_u, "03500000-abcd-1234-efgh-1234567890ab"),
    new MeaSpecMS5803_Temp(&ms5803, "03600000-abcd-1234-efgh-1234567890ab"),
    new MeaSpecMS5803_Pressure(&ms5803, "03700000-abcd-1234-efgh-1234567890ab"),
    new MPL115A2_Temp(&mpl115a2, "03800000-abcd-1234-efgh-1234567890ab"),
    new MPL115A2_Pressure(&mpl115a2, "03900000-abcd-1234-efgh-1234567890ab"),
    // new PaleoTerraRedox_Volt(&redox1, "04000000-abcd-1234-efgh-1234567890ab"),
    // new PaleoTerraRedox_Volt(&redox2, "04100000-abcd-1234-efgh-1234567890ab"),
    // new PaleoTerraRedox_Volt(&redox3, "04200000-abcd-1234-efgh-1234567890ab"),
    new RainCounterI2C_Tips(&tbi2c, "04300000-abcd-1234-efgh-1234567890ab"),
    new RainCounterI2C_Depth(&tbi2c, "04400000-abcd-1234-efgh-1234567890ab"),
    new TIINA219_Current  (&ina219, "08100000-abcd-1234-efgh-1234567890ab"),
    new TIINA219_Volt(&ina219, "08200000-abcd-1234-efgh-1234567890ab"),
    new TIINA219_Power  (&ina219, "08300000-abcd-1234-efgh-1234567890ab"),
    new KellerAcculevel_Pressure(&acculevel, "04500000-abcd-1234-efgh-1234567890ab"),
    new KellerAcculevel_Temp(&acculevel, "04600000-abcd-1234-efgh-1234567890ab"),
    new KellerAcculevel_Height(&acculevel, "04700000-abcd-1234-efgh-1234567890ab"),
    new KellerNanolevel_Pressure(&nanolevel, "04800000-abcd-1234-efgh-1234567890ab"),
    new KellerNanolevel_Temp(&nanolevel, "04900000-abcd-1234-efgh-1234567890ab"),
    new KellerNanolevel_Height(&nanolevel, "05000000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY504_DOpct(&y504, "05100000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY504_Temp(&y504, "05200000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY504_DOmgL(&y504, "05300000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY510_Temp(&y510, "05400000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY510_Turbidity(&y510, "05500000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY511_Temp(&y511, "05600000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY511_Turbidity(&y511, "05700000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY514_Temp(&y514, "05800000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY514_Chlorophyll(&y514, "05900000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY520_Temp(&y520, "06000000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY520_Cond(&y520, "06100000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY532_Temp(&y532, "06200000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY532_Voltage(&y532, "06300000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY532_pH(&y532, "06400000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_DOmgL(&y4000, "06500000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_Turbidity(&y4000, "06600000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_Cond(&y4000, "06700000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_pH(&y4000, "06800000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_Temp(&y4000, "06900000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_ORP(&y4000, "07000000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_Chlorophyll(&y4000, "07100000-abcd-1234-efgh-1234567890ab"),
    new YosemitechY4000_BGA(&y4000, "07200000-abcd-1234-efgh-1234567890ab"),
    new ZebraTechDOpto_Temp(&dopto, "07300000-abcd-1234-efgh-1234567890ab"),
    new ZebraTechDOpto_DOpct(&dopto, "07400000-abcd-1234-efgh-1234567890ab"),
    new ZebraTechDOpto_DOmgL(&dopto, "07500000-abcd-1234-efgh-1234567890ab"),
    new ProcessorStats_FreeRam(&feather, "07600000-abcd-1234-efgh-1234567890ab"),
    new ProcessorStats_Batt(&feather, "07700000-abcd-1234-efgh-1234567890ab"),
    new MaximDS3231_Temp(&ds3231, "07800000-abcd-1234-efgh-1234567890ab"),
    new Modem_RSSI(&modem, "07900000-abcd-1234-efgh-1234567890ab"),
    new Modem_SignalPercent(&modem, "08000000-abcd-1234-efgh-1234567890ab"),
    // new YOUR_variableName_HERE(&)
};
// Count up the number of pointers in the array
int variableCount = sizeof(variableList) / sizeof(variableList[0]);
// Create the VariableArray object
VariableArray varArray(variableCount, variableList);

// Create a new logger instance
#include <LoggerBase.h>
Logger dataLogger(LoggerID, loggingInterval, sdCardPin, wakePin, &varArray);


// ==========================================================================
// Device registration and sampling feature information
//   This should be obtained after registration at http://data.envirodiy.org
// ==========================================================================
const char *registrationToken = "12345678-abcd-1234-efgh-1234567890ab";   // Device registration token
const char *samplingFeature = "12345678-abcd-1234-efgh-1234567890ab";     // Sampling feature UUID

// Create a data publisher for the EnviroDIY/WikiWatershed POST endpoint
#include <publishers/EnviroDIYPublisher.h>
EnviroDIYPublisher EnviroDIYPOST(dataLogger, registrationToken, samplingFeature);

#ifdef DreamHostPortalRX
// Create a data publisher to DreamHost
#include <publishers/DreamHostPublisher.h>
DreamHostPublisher DreamHostGET(dataLogger, DreamHostPortalRX);
#endif


// ==========================================================================
// ThingSpeak Information
// ==========================================================================
const char *thingSpeakMQTTKey = "xxxxxxxxxxxxxxxx";  // Your MQTT API Key from Account > MyProfile.
const char *thingSpeakChannelID = "######";  // The numeric channel id for your channel
const char *thingSpeakChannelKey = "xxxxxxxxxxxxxxxx";  // The Write API Key for your channel

#include <publishers/ThingSpeakPublisher.h>
ThingSpeakPublisher ThingSpeakMQTT(dataLogger, thingSpeakMQTTKey, thingSpeakChannelID, thingSpeakChannelKey);


// ==========================================================================
//    Working Functions
// ==========================================================================

// Flashes the LED's on the primary board
void greenredflash(uint8_t numFlash = 4, uint8_t rate = 75)
{
  for (uint8_t i = 0; i < numFlash; i++) {
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);
    delay(rate);
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, HIGH);
    delay(rate);
  }
  digitalWrite(redLED, LOW);
}


// ==========================================================================
// Main setup function
// ==========================================================================
void setup()
{
    // Wait for USB connection to be established by PC
    // NOTE:  Only use this when debugging - if not connected to a PC, this
    // will prevent the script from starting
    while (!Serial){}

    // Start the primary serial connection
    Serial.begin(serialBaud);

    // Print a start-up note to the first serial port
    Serial.print(F("Now running "));
    Serial.print(sketchName);
    Serial.print(F(" on Logger "));
    Serial.println(LoggerID);
    Serial.println();

    Serial.print(F("Using ModularSensors Library version "));
    Serial.println(MODULAR_SENSORS_VERSION);

    if (String(MODULAR_SENSORS_VERSION) !=  String(libraryVersion))
        Serial.println(F(
            "WARNING: THIS EXAMPLE WAS WRITTEN FOR A DIFFERENT VERSION OF MODULAR SENSORS!!"));

    // Start the serial connection with the modem
    ModemSerial.begin(ModemBaud);

    // Start the stream for the modbus sensors
    modbusSerial.begin(9600);

    // Start the SoftwareSerial stream for the sonar
    sonarSerial.begin(9600);

    // Assign pins SERCOM functionality
    pinPeripheral(2, PIO_SERCOM); // Serial3 Tx = SERCOM2 Pad #2
    pinPeripheral(5, PIO_SERCOM);  // Serial3 Rx = SERCOM2 Pad #3
    pinPeripheral(10, PIO_SERCOM);  // Serial2 Tx = SERCOM1 Pad #2
    pinPeripheral(11, PIO_SERCOM);  // Serial2 Rx = SERCOM1 Pad #0

    // Set up pins for the LED's
    pinMode(greenLED, OUTPUT);
    digitalWrite(greenLED, LOW);
    pinMode(redLED, OUTPUT);
    digitalWrite(redLED, LOW);
    // Blink the LEDs to show the board is on and starting up
    greenredflash();

    // Set up some of the power pins so the board boots up with them off
    // NOTE:  This isn't necessary at all.  The logger begin() function
    // should leave all power pins off when it finishes.
    if (modemVccPin >= 0)
    {
        pinMode(modemVccPin, OUTPUT);
        digitalWrite(modemVccPin, LOW);
    }
    if (sensorPowerPin >= 0)
    {
        pinMode(sensorPowerPin, OUTPUT);
        digitalWrite(sensorPowerPin, LOW);
    }

    // Set up the sleep/wake pin for the modem and put its inital value as "off"
    #if defined(TINY_GSM_MODEM_XBEE)
        Serial.println(F("Setting up sleep mode on the XBee."));
        pinMode(modemSleepRqPin, OUTPUT);
        digitalWrite(modemSleepRqPin, LOW);  // Turn it on to talk, just in case
        tinyModem->init();  // initialize
        if (tinyModem->commandMode())
        {
            tinyModem->sendAT(F("SM"),1);  // Pin sleep
            tinyModem->waitResponse();
            tinyModem->sendAT(F("DO"),0);  // Disable remote manager
            tinyModem->waitResponse();
            tinyModem->sendAT(F("SO"),0);  // For Cellular - disconnected sleep
            tinyModem->waitResponse();
            tinyModem->sendAT(F("SO"),200);  // For WiFi - Disassociate from AP for Deep Sleep
            tinyModem->waitResponse();
            tinyModem->writeChanges();
            tinyModem->exitCommand();
        }
        digitalWrite(modemSleepRqPin, HIGH);  // back to sleep
    #elif defined(TINY_GSM_MODEM_ESP8266)
        if (modemSleepRqPin >= 0)
        {
            pinMode(modemSleepRqPin, OUTPUT);
            digitalWrite(modemSleepRqPin, HIGH);
        }
        if (modemResetPin >= 0)
        {
            pinMode(modemResetPin, OUTPUT);
            digitalWrite(modemResetPin, HIGH);
        }
    #elif defined(TINY_GSM_MODEM_UBLOX)
        pinMode(modemSleepRqPin, OUTPUT);
        digitalWrite(modemSleepRqPin, HIGH);
    #else
        pinMode(modemSleepRqPin, OUTPUT);
        digitalWrite(modemSleepRqPin, LOW);
    #endif

    // Set the timezone and offsets
    // Logging in the given time zone
    Logger::setTimeZone(timeZone);
    // Offset is the same as the time zone because the RTC is in UTC
    Logger::setTZOffset(timeZone);

    // Attach the modem and information pins to the logger
    dataLogger.attachModem(modem);
    dataLogger.setAlertPin(greenLED);
    dataLogger.setTestingModePin(buttonPin);

    // Begin the logger
    dataLogger.begin();

    // Run "testing" mode
    dataLogger.testingMode();
}


// ==========================================================================
// Main loop function
// ==========================================================================
void loop()
{
    // Log the data
    dataLogger.logDataAndSend();
}