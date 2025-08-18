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
#include "esp_sleep.h"
#include <Preferences.h>
#include "SharedDefs.h"
#include <RTClib.h>

// Forward declarations
class DateTime;

// Pin Definitions
#define PIN_SD_CS 12    // SD card chip select
#define PIN_SD_DET 11   // SD card detection pin
#define PIN_FRONT_LED 9 // On-board green LED
// switches are HIGH (INPUT_PULLUP/open) when UP (toward the A/B markers)
// switches are LOW (shunted to gnd/closed) when DOWN (away from the A/B markers)
// deploy to production in UP position for minimal current draw
#define PIN_SWITCH_A 6   // Switch A
#define PIN_SWITCH_B 5   // Switch B
#define PIN_SD_PWR_EN 10 // SD card VDD LDO enable
#define TP_1 8
#define TP_2 15

// NeoPixel Colors
#define NEOPIXEL_OFF 0x000000
#define NEOPIXEL_RED 0xFF0000
#define NEOPIXEL_GREEN 0x00FF00
#define NEOPIXEL_BLUE 0x0000FF
#define NEOPIXEL_PURPLE 0x800080
#define NEOPIXEL_WHITE 0xFFFFFF
#define NEOPIXEL_DIM 3

// Environmental constants
#define SEALEVELPRESSURE_HPA (1013.25)

// Rules
#define LOW_BATTERY_THRESHOLD 3.7

// Library Version
#define HUBLINK_BEAM_VERSION "2.1.0"

// CSV Header
#define CSV_HEADER "datetime,millis,device_id,library_version,battery_voltage,temperature_c,pressure_hpa,humidity_percent,lux,activity_count,activity_percent,inactivity_period_s,inactivity_count,inactivity_percent,min_free_heap,reboot"

class HublinkBEAM
{
public:
    HublinkBEAM();
    bool begin();
    bool initSD();
    bool logData();
    void sleep(uint32_t minutes);

    // File creation behavior
    void setNewFileOnBoot(bool value) { _newFileOnBoot = value; }
    bool getNewFileOnBoot() { return _newFileOnBoot; }

    // Device ID control
    void setDeviceID(String deviceID);
    String getDeviceID() { return _deviceID; }

    // Inactivity period control
    void setInactivityPeriod(uint16_t seconds) { _inactivityPeriod = seconds; }
    uint16_t getInactivityPeriod() { return _inactivityPeriod; }

    // Alarm randomization control
    void setAlarmRandomization(uint16_t minutes) { _alarmRandomizationMinutes = minutes; }
    uint16_t getAlarmRandomization() { return _alarmRandomizationMinutes; }

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

    // Alarm function: if minutes > 0, sets/updates alarm; if minutes = 0, checks if alarm triggered
    bool alarm(uint16_t minutes = 0);

    bool switchADown();
    bool switchBDown();
    bool isWakeFromSleep() { return _isWakeFromSleep; }

private:
    void initPins();
    bool initSensors(bool isWakeFromSleep);
    String getCurrentFilename();      // Gets filename in YYYYMMDD.csv format
    bool createFile(String filename); // Creates new file with header
    bool isSDCardPresent();           // Checks if SD card is inserted
    void enableSDPower();
    void disableSDPower();
    uint32_t hashMacAddress(); // Generate hash from MAC address for randomization
    bool _isLowBattery;
    bool _isWakeFromSleep;                   // Track wake state
    bool _newFileOnBoot = true;              // Controls whether to create new file on each boot
    String _deviceID = "XXX";                // Device ID for filename (3 characters)
    double _pir_percent_active;              // Track PIR activity as fraction of sleep time
    double _inactivity_fraction;             // Track inactivity as fraction of possible periods
    uint16_t _inactivityPeriod = 0;          // Inactivity period in seconds (0 = disabled)
    uint16_t _alarmRandomizationMinutes = 0; // Alarm randomization in minutes (0 = disabled)
    uint32_t _minFreeHeap;                   // Track minimum free heap
    uint32_t _elapsed_seconds;               // Store elapsed time for inactivity calculations
    double _active_seconds;                  // Store active time for inactivity calculations
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
    Preferences _preferences;
};

#endif