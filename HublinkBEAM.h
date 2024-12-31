#ifndef HUBLINK_BEAM_H
#define HUBLINK_BEAM_H

#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include "src/ZDP323.h"
#include "Adafruit_MAX1704X.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Adafruit_VEML7700.h"
#include "src/RTCManager.h"

// Pin Definitions
#define PIN_SD_CS 12    // SD card chip select
#define PIN_SD_DET 11   // SD card detection pin
#define PIN_GREEN_LED 5 // On-board green LED

// Environmental constants
#define SEALEVELPRESSURE_HPA (1013.25)

// CSV Header
#define CSV_HEADER "datetime,millis,battery_voltage,temperature_c,pressure_hpa,humidity_percent,lux,pir_count"

class HublinkBEAM
{
public:
    HublinkBEAM(uint8_t pirEnablePin = -1);
    bool begin();
    bool initSD();
    bool logData();

    // Power management functions
    void light_sleep(uint32_t milliseconds); // Light sleep mode
    void deep_sleep(uint32_t milliseconds);  // Deep sleep mode

    // Battery monitoring functions
    float getBatteryVoltage();
    float getBatteryPercent();
    bool isBatteryConnected();

    // Environmental monitoring functions
    float getTemperature(); // Returns temperature in Celsius
    float getPressure();    // Returns pressure in hPa
    float getHumidity();    // Returns relative humidity in %
    float getAltitude();    // Returns approximate altitude in meters
    bool isEnvironmentalSensorConnected();

    // Light sensor functions
    float getLux();         // Returns light level in lux
    uint16_t getRawALS();   // Returns raw ambient light sensor value
    uint16_t getRawWhite(); // Returns raw white light value
    bool isLightSensorConnected();

    // Light sensor configuration
    void setLightGain(uint8_t gain);            // Set ALS gain (VEML7700_GAIN_*)
    void setLightIntegrationTime(uint8_t time); // Set integration time (VEML7700_IT_*)

    // RTC functions
    DateTime getDateTime();
    float getRTCTemperature();
    String getDayOfWeek();
    uint32_t getUnixTime();
    void adjustRTC(uint32_t timestamp);
    void adjustRTC(const DateTime &dt);
    DateTime getFutureTime(int days, int hours, int minutes, int seconds);
    bool isRTCConnected();

private:
    void initPins();
    String getCurrentFilename();      // Gets filename in YYYYMMDD.csv format
    bool createFile(String filename); // Creates new file with header
    bool isSDCardPresent();           // Checks if SD card is inserted

    File _dataFile;
    bool _isSDInitialized;
    ZDP323 _pirSensor;
    bool _isPIRInitialized;
    Adafruit_MAX17048 _batteryMonitor;
    bool _isBatteryMonitorInitialized;
    Adafruit_BME280 _envSensor;
    bool _isEnvSensorInitialized;
    Adafruit_VEML7700 _lightSensor;
    bool _isLightSensorInitialized;
    RTCManager _rtc;
    bool _isRTCInitialized;
};

#endif