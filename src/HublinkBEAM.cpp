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

bool HublinkBEAM::begin()
{
    bool allInitialized = true;

    // Initialize pins and set NeoPixel to blue during initialization
    initPins();
    setNeoPixel(NEOPIXEL_BLUE);

    Serial.println("Initializing BEAM (give me a moment)...");

    // Initialize I2C for sensors
    Wire.begin();

    // Initialize PIR sensor
    if (!_pirSensor.begin(Wire))
    {
        allInitialized = false;
    }
    else
    {
        _isPIRInitialized = true;
    }

    // Initialize battery monitor
    if (!_batteryMonitor.begin(&Wire))
    {
        allInitialized = false;
    }
    else
    {
        _isBatteryMonitorInitialized = true;
    }

    // Initialize environmental sensor
    if (!_envSensor.begin())
    {
        allInitialized = false;
    }
    else
    {
        _isEnvSensorInitialized = true;
    }

    // Initialize light sensor
    if (!_lightSensor.begin())
    {
        allInitialized = false;
    }
    else
    {
        _isLightSensorInitialized = true;
    }

    // Initialize RTC
    if (!_rtc.begin())
    {
        allInitialized = false;
    }
    else
    {
        _isRTCInitialized = true;
    }

    // Initialize SD card
    if (!initSD())
    {
        allInitialized = false;
    }

    // Print initialization summary
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

    // Set final NeoPixel state based on initialization result
    if (allInitialized)
    {
        disableNeoPixel(); // Turn off if everything is OK
        // Enable motion detection only after all I2C communication is done
        enableMotionDetection();
    }
    else
    {
        setNeoPixel(NEOPIXEL_RED); // Red if there were any failures
    }

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
    // Disable motion detection before any I2C operations
    disableMotionDetection();

    // Get and store motion state ONCE
    bool motionDetected = _isPIRInitialized ? isMotionDetected() : false;

    // Set NeoPixel color based on stored motion state
    setNeoPixel(motionDetected ? NEOPIXEL_GREEN : NEOPIXEL_BLUE);

    // Check for required sensors and SD card
    if (!_isSDInitialized || !isSDCardPresent())
    {
        Serial.println("Cannot log: SD card not initialized or not present");
        if (!_isSDInitialized)
            setNeoPixel(NEOPIXEL_RED);
        else
            disableNeoPixel();
        enableMotionDetection(); // Re-enable motion detection before returning
        return false;
    }

    if (!_isRTCInitialized)
    {
        Serial.println("Cannot log: RTC not initialized");
        if (!_isRTCInitialized)
            setNeoPixel(NEOPIXEL_RED);
        else
            disableNeoPixel();
        enableMotionDetection(); // Re-enable motion detection before returning
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
             motionDetected ? 1UL : 0UL); // Use the stored motion state

    // Write data
    Serial.println(currentFile + ": " + dataString);
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

    // Re-enable motion detection before returning
    enableMotionDetection();
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
    if (_isPIRInitialized)
    {
        gpio_set_direction((gpio_num_t)PIN_PIR_TRIGGER, GPIO_MODE_INPUT);
        gpio_wakeup_enable((gpio_num_t)PIN_PIR_TRIGGER, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
    }

    uint32_t start = millis();
    esp_sleep_enable_timer_wakeup(milliseconds * 1000); // Convert to microseconds

    while ((millis() - start) < milliseconds)
    {
        esp_light_sleep_start();

        // If we woke up early due to motion
        if (_isPIRInitialized && _pirSensor.isMotionDetected())
        {
            // Motion has been detected and interrupt already disabled by ZDP323
            // Calculate remaining sleep time
            uint32_t elapsed = millis() - start;
            if (elapsed < milliseconds)
            {
                uint32_t remaining = milliseconds - elapsed;
                // Reset wakeup timer for remaining duration
                esp_sleep_enable_timer_wakeup(remaining * 1000);
            }
        }
    }

    // Re-enable motion detection if it was disabled
    if (_isPIRInitialized)
    {
        enableMotionDetection();
    }
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

void HublinkBEAM::enableMotionDetection()
{
    if (_isPIRInitialized)
    {
        _pirSensor.enableInterrupt(PIN_PIR_TRIGGER);
    }
}

void HublinkBEAM::disableMotionDetection()
{
    if (_isPIRInitialized)
    {
        _pirSensor.disableInterrupt(PIN_PIR_TRIGGER);
    }
}