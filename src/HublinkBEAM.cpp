#include "HublinkBEAM.h"
#include "esp_sleep.h"

// Use RTC memory to maintain state across deep sleep
static RTC_DATA_ATTR bool firstBoot = true;

HublinkBEAM::HublinkBEAM() : _pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800)
{
    _isSDInitialized = false;
    _isPIRInitialized = false;
    _isBatteryMonitorInitialized = false;
    _isEnvSensorInitialized = false;
    _isLightSensorInitialized = false;
    _isRTCInitialized = false;
}

bool HublinkBEAM::isWakeFromDeepSleep()
{
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    return wakeup_reason == ESP_SLEEP_WAKEUP_TIMER;
}

void HublinkBEAM::initPins()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

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

bool HublinkBEAM::initSensors(bool isWakeFromSleep)
{
    bool allInitialized = true;

    Serial.println("   a. Starting I2C...");
    // Release GPIO hold if waking from sleep
    if (isWakeFromSleep)
    {
        _ulp.stop();
    }

    // Initialize I2C for sensors
    Wire.begin();
    delay(100); // Give I2C time to stabilize

    Serial.println("   b. Initializing PIR sensor...");
    // Initialize PIR sensor with optimized init for wake from sleep
    if (!_pirSensor.begin(Wire, isWakeFromSleep))
    {
        Serial.println("      PIR sensor failed");
        allInitialized = false;
    }
    else
    {
        Serial.println("      PIR sensor OK");
        _isPIRInitialized = true;
    }

    // Always initialize I2C devices, but with optimized init for wake from sleep
    Serial.println(isWakeFromSleep ? "   c. Re-initializing I2C devices after sleep..." : "   c. Initializing I2C devices...");

    // Initialize battery monitor
    Serial.println("      - Battery monitor...");
    if (!_batteryMonitor.begin(&Wire))
    {
        Serial.println("        Failed");
        allInitialized = false;
        _isBatteryMonitorInitialized = false;
    }
    else
    {
        Serial.println("        OK");
        _isBatteryMonitorInitialized = true;
    }

    // Initialize environmental sensor
    Serial.println("      - Environmental sensor...");
    if (!_envSensor.begin())
    {
        Serial.println("        Failed");
        allInitialized = false;
        _isEnvSensorInitialized = false;
    }
    else
    {
        Serial.println("        OK");
        _isEnvSensorInitialized = true;
    }

    // Initialize light sensor
    Serial.println("      - Light sensor...");
    if (!_lightSensor.begin())
    {
        Serial.println("        Failed");
        allInitialized = false;
        _isLightSensorInitialized = false;
    }
    else
    {
        Serial.println("        OK");
        _isLightSensorInitialized = true;
    }

    // Initialize RTC
    Serial.println("      - RTC...");
    if (!_rtc.begin())
    {
        Serial.println("        Failed");
        allInitialized = false;
        _isRTCInitialized = false;
    }
    else
    {
        Serial.println("        OK");
        _isRTCInitialized = true;
    }

    return allInitialized;
}

bool HublinkBEAM::begin()
{
    bool allInitialized = true;

    Serial.println("Initializing BEAM...");
    Serial.println("1. Initializing pins...");

    // Initialize pins and set NeoPixel to blue during initialization
    initPins();
    setNeoPixel(NEOPIXEL_BLUE);

    Serial.println("2. Checking wake status...");
    // Check if this is a wake from deep sleep
    bool isWakeFromSleep = isWakeFromDeepSleep();
    Serial.printf("   Wake from sleep: %s\n", isWakeFromSleep ? "YES" : "NO");

    Serial.println("3. Initializing sensors...");
    // Initialize sensors (with optimization if waking from sleep)
    if (!initSensors(isWakeFromSleep))
    {
        Serial.println("   Sensor initialization failed");
        allInitialized = false;
    }
    else
    {
        Serial.println("   Sensors initialized successfully");
    }

    Serial.println("4. Initializing SD card...");
    // Always reinitialize SD card after deep sleep
    if (!initSD())
    {
        Serial.println("   SD card initialization failed");
        allInitialized = false;
    }
    else
    {
        Serial.println("   SD card initialized successfully");
    }

    // Only print full initialization report on first boot
    if (!isWakeFromSleep)
    {
        Serial.println("\nHublink BEAM Initialization Report:");
        Serial.println("--------------------------------");
        Serial.printf("PIR Sensor: %s\n", _isPIRInitialized ? "OK" : "FAILED");
        Serial.printf("Battery Monitor: %s", _isBatteryMonitorInitialized ? "OK" : "FAILED");
        if (_isBatteryMonitorInitialized)
        {
            Serial.printf(" (%.2fV, %.1f%%)",
                          _batteryMonitor.cellVoltage(),
                          _batteryMonitor.cellPercent());
        }
        Serial.println();
        Serial.printf("Environmental Sensor: %s\n", _isEnvSensorInitialized ? "OK" : "FAILED");
        Serial.printf("Light Sensor: %s\n", _isLightSensorInitialized ? "OK" : "FAILED");
        Serial.printf("RTC: %s\n", _isRTCInitialized ? "OK" : "FAILED");
        Serial.printf("SD Card: %s\n", _isSDInitialized ? "OK" : "FAILED");
        Serial.printf("Overall Status: %s\n", allInitialized ? "OK" : "FAILED");
        Serial.println("--------------------------------");
    }

    // Set final NeoPixel state based on initialization result
    if (allInitialized)
    {
        disableNeoPixel(); // Turn off if everything is OK
    }
    else
    {
        setNeoPixel(NEOPIXEL_RED); // Red if there were any failures
    }

    Serial.println("5. Initialization complete.");
    Serial.flush();

    firstBoot = false;
    return allInitialized;
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
    char filename[14];
    snprintf(filename, sizeof(filename), "/%04d%02d%02d.csv",
             now.year(), now.month(), now.day());
    return String(filename);
}

bool HublinkBEAM::createFile(String filename)
{
    // Ensure filename starts with a forward slash
    if (!filename.startsWith("/"))
    {
        filename = "/" + filename;
    }

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
    Serial.println("\n\n---Logging data---");
    // Get motion flag from ULP first
    int motionFlag = _ulp.getEventFlag() ? 1 : 0;
    _ulp.clearEventFlag(); // Clear after reading

    // Set initial NeoPixel color based on motion
    setNeoPixel(motionFlag ? NEOPIXEL_GREEN : NEOPIXEL_PURPLE);

    // Check for required sensors and SD card
    if (!_isSDInitialized || !isSDCardPresent())
    {
        Serial.println("Cannot log: SD card not initialized or not present");
        setNeoPixel(NEOPIXEL_RED);
        return false;
    }

    if (!_isRTCInitialized)
    {
        Serial.println("Cannot log: RTC not initialized");
        setNeoPixel(NEOPIXEL_RED);
        return false;
    }

    String currentFile = filename ? String(filename) : getCurrentFilename();

    // Ensure filename starts with a forward slash
    if (!currentFile.startsWith("/"))
    {
        currentFile = "/" + currentFile;
    }

    // Check if file exists, create it with header if it doesn't
    if (!SD.exists(currentFile))
    {
        if (!createFile(currentFile))
        {
            setNeoPixel(NEOPIXEL_RED);
            return false;
        }
    }

    // Open file in append mode
    File dataFile = SD.open(currentFile, FILE_APPEND);
    if (!dataFile)
    {
        Serial.println("Failed to open file for logging: " + currentFile);
        setNeoPixel(NEOPIXEL_RED);
        return false;
    }

    // Get date/time and sensor readings
    DateTime now = getDateTime();
    float batteryV = _isBatteryMonitorInitialized ? getBatteryVoltage() : -1.0f;
    float tempC = _isEnvSensorInitialized ? getTemperature() : -273.15f;
    float pressHpa = _isEnvSensorInitialized ? getPressure() : -1.0f;
    float humidity = _isEnvSensorInitialized ? getHumidity() : -1.0f;
    float lux = _isLightSensorInitialized ? getLux() : -1.0f;

    // Format data string
    char dataString[128];
    snprintf(dataString, sizeof(dataString),
             "%04d-%02d-%02d %02d:%02d:%02d,%lu,%.3f,%.2f,%.2f,%.2f,%.2f,%d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second(),
             millis(),
             batteryV,
             tempC,
             pressHpa,
             humidity,
             lux,
             motionFlag);

    // Write data
    bool success = dataFile.println(dataString);
    dataFile.close();

    Serial.println(currentFile + ": " + dataString);

    if (success)
    {
        disableNeoPixel(); // Turn off if everything was OK
    }
    else
    {
        Serial.println("Failed to write to file: " + currentFile);
        setNeoPixel(NEOPIXEL_RED); // Show error state
    }
    Serial.flush();

    return success;
}

void HublinkBEAM::sleep(uint32_t milliseconds)
{
    disableNeoPixel();
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(PIN_GREEN_LED, LOW);

    // Enable trigger mode for PIR sensor before sleep
    if (_isPIRInitialized)
    {
        Serial.println("      - Enabling PIR trigger mode...");
        _pirSensor.enableTriggerMode();
    }

    // Start ULP program
    _ulp.start();

    // Configure deep sleep wakeup sources
    Serial.println("      - Enabling ULP wakeup...");
    ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());     // Enable ULP wakeup
    esp_sleep_enable_timer_wakeup(milliseconds * 1000); // Convert to microseconds

    Serial.println("      - Starting deep sleep...");
    esp_deep_sleep_start();
    // Note: Device will restart after deep sleep
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