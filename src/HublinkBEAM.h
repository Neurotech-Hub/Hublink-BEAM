#ifndef HUBLINK_BEAM_H
#define HUBLINK_BEAM_H

#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include "ZDP323.h"
#include "Adafruit_MAX1704X.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Adafruit_VEML7700.h"
#include "RTCManager.h"
#include "ULPManager.h"
#include <Adafruit_NeoPixel.h>
#include "esp_sleep.h"`

// Pin Definitions
#define PIN_SD_CS 12    // SD card chip select
#define PIN_SD_DET 11   // SD card detection pin
#define PIN_GREEN_LED 5 // On-board green LED
#define NEOPIXEL_DIM 3

// NeoPixel Colors
#define NEOPIXEL_OFF 0x000000
#define NEOPIXEL_RED 0xFF0000
#define NEOPIXEL_GREEN 0x00FF00
#define NEOPIXEL_BLUE 0x0000FF
#define NEOPIXEL_PURPLE 0x800080

// Environmental constants
#define SEALEVELPRESSURE_HPA (1013.25)

// CSV Header
#define CSV_HEADER "datetime,millis,battery_voltage,temperature_c,pressure_hpa,humidity_percent,lux,pir_count"

class HublinkBEAM
{
public:
    HublinkBEAM();
    bool begin();
    bool initSD();
    bool logData(const char *filename = nullptr);
    void sleep(uint32_t seconds); // Update parameter name in declaration

    // NeoPixel control functions
    void setNeoPixel(uint32_t color);
    void disableNeoPixel();

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
    String getDayOfWeek();
    uint32_t getUnixTime();
    void adjustRTC(uint32_t timestamp);
    void adjustRTC(const DateTime &dt);
    DateTime getFutureTime(int days, int hours, int minutes, int seconds);
    bool isRTCConnected();

private:
    void initPins();
    bool initSensors(bool isWakeFromSleep);
    String getCurrentFilename();      // Gets filename in YYYYMMDD.csv format
    bool createFile(String filename); // Creates new file with header
    bool isSDCardPresent();           // Checks if SD card is inserted
    bool isWakeFromDeepSleep();       // Check if we're waking from deep sleep

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
    Adafruit_NeoPixel _pixel;
    ULPManager _ulp;
};

#endif