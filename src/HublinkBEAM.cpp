#include "HublinkBEAM.h"

HublinkBEAM::HublinkBEAM() : _pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800)
{
    _isSDInitialized = false;
    _isPIRInitialized = false;
    _isBatteryMonitorInitialized = false;
    _isEnvSensorInitialized = false;
    _isLightSensorInitialized = false;
    _isRTCInitialized = false;
}

void HublinkBEAM::initPins()
{
    // Initialize SD card pins
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH); // Deselect SD card by default
    pinMode(PIN_SD_DET, INPUT_PULLUP);

    // Initialize on-board LED
    pinMode(PIN_GREEN_LED, OUTPUT);
    digitalWrite(PIN_GREEN_LED, LOW); // Turn off green LED

    // Initialize NeoPixel power pin and pixel
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
    _pixel.begin();
    _pixel.setBrightness(20); // Set to low brightness
    _pixel.show();            // Initialize all pixels to 'off'
}

void HublinkBEAM::setNeoPixel(uint32_t color)
{
    digitalWrite(NEOPIXEL_POWER, HIGH);
    _pixel.setPixelColor(0, color);
    _pixel.show();
}

void HublinkBEAM::disableNeoPixel()
{
    _pixel.setPixelColor(0, NEOPIXEL_OFF);
    _pixel.show();
    digitalWrite(NEOPIXEL_POWER, LOW);
}

bool HublinkBEAM::begin()
{
    bool allInitialized = true;
    Serial.println("\nInitializing Hublink BEAM...");
    Serial.println("---------------------------");

    // Initialize pins and set NeoPixel to blue during initialization
    initPins();
    setNeoPixel(NEOPIXEL_BLUE);
    Serial.println("Pins initialized");

    // Initialize I2C for sensors
    Wire.begin();
    Serial.println("I2C initialized");

    // Initialize PIR sensor
    Serial.print("Initializing PIR sensor... ");
    if (!_pirSensor.begin(Wire))
    {
        Serial.println("FAILED");
        allInitialized = false;
    }
    else
    {
        _isPIRInitialized = true;
        Serial.println("OK");
    }

    // Initialize battery monitor
    Serial.print("Initializing battery monitor... ");
    if (!_batteryMonitor.begin(&Wire))
    {
        Serial.println("FAILED");
        Serial.println("Could not find a valid MAX17048 sensor");
        allInitialized = false;
    }
    else
    {
        _isBatteryMonitorInitialized = true;
        Serial.println("OK");
        Serial.printf("Battery: %.2fV (%.1f%%)\n",
                      _batteryMonitor.cellVoltage(),
                      _batteryMonitor.cellPercent());
    }

    // Initialize environmental sensor
    Serial.print("Initializing environmental sensor... ");
    if (!_envSensor.begin())
    {
        Serial.println("FAILED");
        allInitialized = false;
    }
    else
    {
        _isEnvSensorInitialized = true;
        Serial.println("OK");
    }

    // Initialize light sensor
    Serial.print("Initializing light sensor... ");
    if (!_lightSensor.begin())
    {
        Serial.println("FAILED");
        allInitialized = false;
    }
    else
    {
        _isLightSensorInitialized = true;
        Serial.println("OK");
    }

    // Initialize RTC
    Serial.print("Initializing RTC... ");
    if (!_rtc.begin())
    {
        Serial.println("FAILED");
        allInitialized = false;
    }
    else
    {
        _isRTCInitialized = true;
        Serial.println("OK");
    }

    // Initialize SD card
    Serial.print("Initializing SD card... ");
    if (!initSD())
    {
        Serial.println("FAILED");
        allInitialized = false;
    }
    else
    {
        Serial.println("OK");
    }

    Serial.println("\nInitialization Summary:");
    Serial.println("----------------------");
    Serial.printf("PIR Sensor: %s\n", _isPIRInitialized ? "OK" : "FAILED");
    Serial.printf("Battery Monitor: %s\n", _isBatteryMonitorInitialized ? "OK" : "FAILED");
    Serial.printf("Environmental Sensor: %s\n", _isEnvSensorInitialized ? "OK" : "FAILED");
    Serial.printf("Light Sensor: %s\n", _isLightSensorInitialized ? "OK" : "FAILED");
    Serial.printf("RTC: %s\n", _isRTCInitialized ? "OK" : "FAILED");
    Serial.printf("SD Card: %s\n", _isSDInitialized ? "OK" : "FAILED");
    Serial.println("----------------------");

    // Set final NeoPixel state based on initialization result
    if (allInitialized)
    {
        disableNeoPixel(); // Turn off if everything is OK
    }
    else
    {
        setNeoPixel(NEOPIXEL_RED); // Red if there were any failures
    }

    return allInitialized;
}

bool HublinkBEAM::initSD()
{
    if (!isSDCardPresent())
    {
        Serial.println("No SD card detected!");
        return false;
    }

    if (!SD.begin(PIN_SD_CS))
    {
        Serial.println("SD card initialization failed!");
        return false;
    }

    _isSDInitialized = true;
    Serial.println("SD card initialized successfully");
    return true;
}

bool HublinkBEAM::isSDCardPresent()
{
    return !digitalRead(PIN_SD_DET); // Pin is pulled up, so LOW means card is present
}

String HublinkBEAM::getCurrentFilename()
{
    DateTime now = getDateTime();
    char filename[13];
    snprintf(filename, sizeof(filename), "%04d%02d%02d.csv",
             now.year(), now.month(), now.day());
    return String(filename);
}

bool HublinkBEAM::createFile(String filename)
{
    File file = SD.open(filename, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to create file: " + filename);
        return false;
    }

    // Write header row
    if (file.println(CSV_HEADER))
    {
        Serial.println("Created new log file: " + filename);
        file.close();
        return true;
    }

    Serial.println("Failed to write header to file: " + filename);
    file.close();
    return false;
}

bool HublinkBEAM::logData(const char *filename)
{
    // Set NeoPixel to blue to indicate logging activity
    setNeoPixel(NEOPIXEL_BLUE);

    // Check for required sensors and SD card
    if (!_isSDInitialized || !isSDCardPresent())
    {
        Serial.println("Cannot log: SD card not initialized or not present");
        if (!_isSDInitialized)
            setNeoPixel(NEOPIXEL_RED); // Keep red if initialization failed
        else
            disableNeoPixel(); // Turn off if everything was initialized OK
        return false;
    }

    if (!_isRTCInitialized)
    {
        Serial.println("Cannot log: RTC not initialized");
        if (!_isRTCInitialized)
            setNeoPixel(NEOPIXEL_RED); // Keep red if initialization failed
        else
            disableNeoPixel(); // Turn off if everything was initialized OK
        return false;
    }

    String currentFile = filename ? String(filename) : getCurrentFilename();

    // Check if file exists, create it with header if it doesn't
    if (!SD.exists(currentFile))
    {
        if (!createFile(currentFile))
        {
            setNeoPixel(NEOPIXEL_RED); // Show error state
            return false;
        }
    }

    // Open file in append mode
    File dataFile = SD.open(currentFile, FILE_APPEND);
    if (!dataFile)
    {
        Serial.println("Failed to open file for logging: " + currentFile);
        setNeoPixel(NEOPIXEL_RED); // Show error state
        return false;
    }

    // Get motion state (1 if motion detected, 0 if not)
    uint32_t motionState = _isPIRInitialized ? (isMotionDetected() ? 1 : 0) : 0;

    // Format data row with error values for uninitialized sensors
    DateTime now = getDateTime();
    char dataString[128];
    snprintf(dataString, sizeof(dataString),
             "%04d-%02d-%02d %02d:%02d:%02d,%lu,%.3f,%.2f,%.2f,%.2f,%.2f,%lu",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second(),
             millis(),
             _isBatteryMonitorInitialized ? getBatteryVoltage() : -1.0f,
             _isEnvSensorInitialized ? getTemperature() : -273.15f,
             _isEnvSensorInitialized ? getPressure() : -1.0f,
             _isEnvSensorInitialized ? getHumidity() : -1.0f,
             _isLightSensorInitialized ? getLux() : -1.0f,
             motionState);

    // Write data
    Serial.println("Writing data to file: " + currentFile);
    Serial.println(dataString);
    bool success = dataFile.println(dataString);
    dataFile.close();

    if (success)
    {
        disableNeoPixel(); // Turn off if everything was OK
    }
    else
    {
        Serial.println("Failed to write to file: " + currentFile);
        setNeoPixel(NEOPIXEL_RED); // Show error state
    }

    return success;
}

float HublinkBEAM::getBatteryVoltage()
{
    if (!_isBatteryMonitorInitialized)
    {
        return -1.0;
    }
    return _batteryMonitor.cellVoltage();
}

float HublinkBEAM::getBatteryPercent()
{
    if (!_isBatteryMonitorInitialized)
    {
        return -1.0;
    }
    return _batteryMonitor.cellPercent();
}

bool HublinkBEAM::isBatteryConnected()
{
    if (!_isBatteryMonitorInitialized)
    {
        return false;
    }
    float voltage = _batteryMonitor.cellVoltage();
    return !isnan(voltage);
}

float HublinkBEAM::getTemperature()
{
    if (!_isEnvSensorInitialized)
    {
        return -273.15; // Absolute zero as error value
    }
    return _envSensor.readTemperature();
}

float HublinkBEAM::getPressure()
{
    if (!_isEnvSensorInitialized)
    {
        return -1.0;
    }
    return _envSensor.readPressure() / 100.0F; // Convert Pa to hPa
}

float HublinkBEAM::getHumidity()
{
    if (!_isEnvSensorInitialized)
    {
        return -1.0;
    }
    return _envSensor.readHumidity();
}

float HublinkBEAM::getAltitude()
{
    if (!_isEnvSensorInitialized)
    {
        return -1.0;
    }
    return _envSensor.readAltitude(SEALEVELPRESSURE_HPA);
}

bool HublinkBEAM::isEnvironmentalSensorConnected()
{
    return _isEnvSensorInitialized;
}

float HublinkBEAM::getLux()
{
    if (!_isLightSensorInitialized)
    {
        return -1.0;
    }
    return _lightSensor.readLux();
}

uint16_t HublinkBEAM::getRawALS()
{
    if (!_isLightSensorInitialized)
    {
        return 0;
    }
    return _lightSensor.readALS();
}

uint16_t HublinkBEAM::getRawWhite()
{
    if (!_isLightSensorInitialized)
    {
        return 0;
    }
    return _lightSensor.readWhite();
}

bool HublinkBEAM::isLightSensorConnected()
{
    return _isLightSensorInitialized;
}

void HublinkBEAM::setLightGain(uint8_t gain)
{
    if (_isLightSensorInitialized)
    {
        _lightSensor.setGain(gain);
    }
}

void HublinkBEAM::setLightIntegrationTime(uint8_t time)
{
    if (_isLightSensorInitialized)
    {
        _lightSensor.setIntegrationTime(time);
    }
}

// RTC Functions
DateTime HublinkBEAM::getDateTime()
{
    if (!_isRTCInitialized)
    {
        return DateTime(SECONDS_FROM_1970_TO_2000); // Use RTClib's default epoch
    }
    return _rtc.now();
}

float HublinkBEAM::getRTCTemperature()
{
    if (!_isRTCInitialized)
    {
        return -273.15;
    }
    return _rtc.getTemperature();
}

String HublinkBEAM::getDayOfWeek()
{
    if (!_isRTCInitialized)
    {
        return String();
    }
    return _rtc.getDayOfWeek();
}

uint32_t HublinkBEAM::getUnixTime()
{
    if (!_isRTCInitialized)
    {
        return 0;
    }
    return _rtc.getUnixTime();
}

void HublinkBEAM::adjustRTC(uint32_t timestamp)
{
    if (_isRTCInitialized)
    {
        _rtc.adjustRTC(timestamp);
    }
}

void HublinkBEAM::adjustRTC(const DateTime &dt)
{
    if (_isRTCInitialized)
    {
        _rtc.adjustRTC(dt);
    }
}

DateTime HublinkBEAM::getFutureTime(int days, int hours, int minutes, int seconds)
{
    if (!_isRTCInitialized)
    {
        return DateTime(SECONDS_FROM_1970_TO_2000); // Use RTClib's default epoch
    }
    return _rtc.getFutureTime(days, hours, minutes, seconds);
}

bool HublinkBEAM::isRTCConnected()
{
    return _isRTCInitialized;
}

void HublinkBEAM::light_sleep(uint32_t milliseconds)
{
    esp_sleep_enable_timer_wakeup(milliseconds * 1000); // Convert to microseconds
    esp_light_sleep_start();
}

void HublinkBEAM::deep_sleep(uint32_t milliseconds)
{
    esp_sleep_enable_timer_wakeup(milliseconds * 1000); // Convert to microseconds
    esp_deep_sleep_start();
    // Note: Device will restart after deep sleep
}

bool HublinkBEAM::isMotionDetected()
{
    if (!_isPIRInitialized)
    {
        Serial.println("PIR sensor not initialized");
        return false;
    }
    return _pirSensor.isMotionDetected();
}