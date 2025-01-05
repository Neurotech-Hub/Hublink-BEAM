#include "HublinkBEAM.h"
#include "RTCManager.h"
#include "esp_sleep.h"

// Use RTC memory to maintain state across deep sleep
static RTC_DATA_ATTR bool firstBoot = true;
RTC_DATA_ATTR sleep_config_t sleep_config;

HublinkBEAM::HublinkBEAM() : _pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800)
{
    _isSDInitialized = false;
    _isPIRInitialized = false;
    _isBatteryMonitorInitialized = false;
    _isEnvSensorInitialized = false;
    _isLightSensorInitialized = false;
    _isRTCInitialized = false;
    _isLowBattery = false;
    _isWakeFromSleep = false;
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

    // Initialize and hold (during deep sleep) I2C power control
    pinMode(PIN_I2C_POWER, OUTPUT);
    digitalWrite(PIN_I2C_POWER, HIGH);

    // Initialize NeoPixel power pin and pixel
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
    _pixel.begin();
    _pixel.setBrightness(NEOPIXEL_DIM); // Set to low brightness
    _pixel.show();                      // Initialize all pixels to 'off'
}

bool HublinkBEAM::initSensors(bool isWakeFromSleep)
{
    bool allInitialized = true;
    Serial.println("Initializing sensors...");

    // Always initialize I2C devices, but with optimized init for wake from sleep
    // Initialize battery monitor
    if (!_batteryMonitor.begin(&Wire))
    {
        Serial.println("  Battery: failed");
        allInitialized = false;
        _isBatteryMonitorInitialized = false;
    }
    else
    {
        Serial.println("  Battery: OK");
        _isBatteryMonitorInitialized = true;

        // Check battery level on initial boot
        if (!isWakeFromSleep && _isBatteryMonitorInitialized)
        {
            float voltage = _batteryMonitor.cellVoltage();
            Serial.printf("  Battery voltage: %.2fV\n", voltage);
            if (voltage < LOW_BATTERY_THRESHOLD)
            {
                Serial.printf("  Low battery detected: %.2fV\n", voltage);
                _isLowBattery = true;
                return false;
            }
        }

        if (isWakeFromSleep)
        {
            _batteryMonitor.sleep(false); // Wake from sleep mode
        }
    }

    // Initialize environmental sensor
    if (!_envSensor.begin())
    {
        Serial.println("  BME280: failed");
        allInitialized = false;
        _isEnvSensorInitialized = false;
    }
    else
    {
        Serial.println("  BME280: OK");
        // Configure BME280 for forced mode with 1x oversampling
        _envSensor.setSampling(Adafruit_BME280::MODE_FORCED,
                               Adafruit_BME280::SAMPLING_X1, // temperature
                               Adafruit_BME280::SAMPLING_X1, // pressure
                               Adafruit_BME280::SAMPLING_X1, // humidity
                               Adafruit_BME280::FILTER_OFF);
        _isEnvSensorInitialized = true;
    }

    // Initialize light sensor
    if (!_lightSensor.begin())
    {
        Serial.println("  VEM7700: failed");
        allInitialized = false;
        _isLightSensorInitialized = false;
    }
    else
    {
        Serial.println("  VEM7700: OK");
        _isLightSensorInitialized = true;
    }

    // Initialize RTC
    if (!_rtc.begin())
    {
        Serial.println("  RTC: failed");
        allInitialized = false;
        _isRTCInitialized = false;
    }
    else
    {
        Serial.println("  RTC: OK");
        _isRTCInitialized = true;
    }

    // Initialize PIR sensor with optimized init for wake from sleep
    if (!_pirSensor.begin(Wire, isWakeFromSleep))
    {
        Serial.println("  PIR: failed");
        allInitialized = false;
    }
    else
    {
        Serial.println("  PIR: OK");
        if (!isWakeFromSleep)
        {
            Serial.println("  PIR: starting stabilization");
            // Use regular delay when USB is connected, light sleep otherwise
            if (Serial)
            {
                delay(ZDP323_TSTAB_MS);
            }
            else
            {
                esp_sleep_enable_timer_wakeup(ZDP323_TSTAB_MS * 1000); // Convert ms to microseconds
                esp_light_sleep_start();
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            }
        }
        _isPIRInitialized = true;
    }

    Serial.printf("  All sensors %s\n", allInitialized ? "OK" : "FAILED");
    return allInitialized;
}

bool HublinkBEAM::doDebug()
{
    pinMode(BOOT_GPIO, INPUT_PULLUP);
    return (digitalRead(BOOT_GPIO) == LOW);
}

bool HublinkBEAM::begin()
{
    // Set CPU frequency to 80MHz
    setCpuFrequencyMhz(80);

    // Stop ULP to free up GPIO pins
    _ulp.stop();
    delay(10);

    // Initialize pins and set NeoPixel to blue during initialization
    initPins();
    setNeoPixel(NEOPIXEL_BLUE);

    // Initialize I2C for all cases
    Wire.begin();
    delay(10); // Give I2C time to stabilize
    Serial.println("  I2C: started");

    // Check wakeup reason immediately
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    // If we're in stage 1 (GPIO monitoring) and got a GPIO wakeup
    if (sleep_config.sleep_stage == SLEEP_STAGE_GPIO_MONITOR &&
        wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
    {
        // Quick initialization of RTC for time calculations
        if (!_rtc.begin())
        {
            Serial.println("  Error: Failed to initialize RTC for time calculation");
            return false;
        }
        _isRTCInitialized = true;

        // Increment PIR count for the GPIO interrupt we just received
        RTC_SLOW_MEM[PIR_COUNT]++;

        // Calculate remaining sleep time
        int32_t current_time = getUnixTime();
        int32_t start_time = sleep_config.sleep_start_time;
        int32_t elapsed_time = 0;

        Serial.printf("  Debug - Current time: %d, Start time: %d\n", current_time, start_time);

        // Ensure times are valid and calculate elapsed time
        if (current_time >= start_time)
        {
            elapsed_time = current_time - start_time;
        }
        else
        {
            Serial.println("  Warning: Time calculation error, using full duration");
            elapsed_time = 0;
        }

        uint32_t remaining_time = sleep_config.sleep_duration;
        if (elapsed_time < sleep_config.sleep_duration)
        {
            remaining_time = sleep_config.sleep_duration - elapsed_time;
        }

        Serial.println("\nMotion detected during Stage 1");
        Serial.printf("  Current time: %d\n", current_time);
        Serial.printf("  Start time: %d\n", start_time);
        Serial.printf("  Elapsed time: %d seconds\n", elapsed_time);
        Serial.printf("  Remaining time: %d seconds\n", remaining_time);

        // Move to stage 2: ULP monitoring
        sleep_config.sleep_stage = SLEEP_STAGE_ULP_MONITOR;
        sleep_config.sleep_start_time = current_time;
        sleep_config.sleep_duration = remaining_time; // Update duration to remaining time

        // Start the ULP program (already initialized in sleep())
        _ulp.start();

        Serial.println("Entering Stage 2 Deep Sleep (ULP monitoring)");
        Serial.flush();

        // Configure timer wakeup for remaining time
        uint64_t remaining_microseconds = (uint64_t)remaining_time * 1000000ULL;
        esp_sleep_enable_timer_wakeup(remaining_microseconds);

        esp_deep_sleep_start();
        return false; // Never reached
    }

    // Normal initialization for timer wakeup or regular boot
    Serial.println("Initializing BEAM...");

    bool allInitialized = true;

    // Check if this is a wake from deep sleep
    _isWakeFromSleep = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
    Serial.printf("    Wake from sleep: %s\n", _isWakeFromSleep ? "YES" : "NO");

    // Reset sleep stage on normal boot or timer wakeup
    if (!_isWakeFromSleep || wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
    {
        sleep_config.sleep_stage = SLEEP_STAGE_NORMAL;
    }

    // Always reinitialize SD card after deep sleep
    if (!initSD())
    {
        allInitialized = false;
    }

    // Initialize sensors (with optimization if waking from sleep), skip if SD card failed
    if (!initSensors(_isWakeFromSleep) && allInitialized)
    {
        allInitialized = false;
    }

    // Only print full initialization report on first boot
    if (!_isWakeFromSleep)
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
        if (_isLowBattery)
        {
            setNeoPixel(NEOPIXEL_PURPLE); // Purple for low battery
        }
        else
        {
            setNeoPixel(NEOPIXEL_RED); // Red for other failures
        }
    }

    Serial.flush();
    firstBoot = false;

    // Add debug delay if enabled
    if (doDebug())
    {
        delay(DEBUG_DELAY_MS);
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
    SD.exists("/x.txt"); // trick to enter SD idle state

    _isSDInitialized = true;
    return true;
}

bool HublinkBEAM::isSDCardPresent()
{
    return !digitalRead(PIN_SD_DET); // Pin is pulled up, so LOW means card is present
}

String HublinkBEAM::getCurrentFilename()
{
    DateTime now = getDateTime();
    Serial.println("\nGetting current filename...");
    Serial.printf("  Wake from sleep: %s\n", _isWakeFromSleep ? "YES" : "NO");

    // If waking from sleep, retrieve the stored filename
    if (_isWakeFromSleep)
    {
        _preferences.begin(PREFS_NAMESPACE, false); // read-only mode
        String storedFilename = _preferences.getString("filename", "");
        _preferences.end();
        Serial.printf("  Retrieved from preferences: %s\n", storedFilename.c_str());
        return storedFilename;
    }

    // Hard reboot: create new filename with incremented number
    char baseFilename[7]; // YYMMDD (6 chars + null terminator)
    snprintf(baseFilename, sizeof(baseFilename), "%02d%02d%02d",
             now.year() % 100, now.month(), now.day());
    Serial.printf("  Base filename: %s\n", baseFilename);

    // Scan files to find the highest existing number for today
    uint8_t nextNum = 0;
    char testFilename[14]; // /YYMMDDXX.csv (13 chars + null terminator)

    Serial.println("  Scanning for existing files:");
    // Test each possible number (00-99)
    while (nextNum < 100)
    {
        snprintf(testFilename, sizeof(testFilename), "/%s%02d.csv", baseFilename, nextNum);
        Serial.printf("    Testing: %s - ", testFilename);
        if (!SD.exists(testFilename))
        {
            Serial.println("available");
            break;
        }
        Serial.println("exists");
        nextNum++;
    }

    // Create final filename with the next available number
    char filename[14]; // /YYMMDDXX.csv (13 chars + null terminator)
    snprintf(filename, sizeof(filename), "/%s%02d.csv", baseFilename, nextNum);
    Serial.printf("  New filename: %s\n", filename);

    // Store the new filename in preferences immediately
    _preferences.begin(PREFS_NAMESPACE, false); // read-write mode
    _preferences.putString("filename", String(filename));
    _preferences.end();
    Serial.println("  Stored in preferences");

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

bool HublinkBEAM::logData()
{
    uint16_t motionCount = _ulp.getPIRCount();
    _ulp.clearPIRCount(); // Reset counter after reading

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

    String currentFile = getCurrentFilename();

    // Check if file exists, create it with header if it doesn't
    if (!SD.exists(currentFile))
    {
        if (!createFile(currentFile))
        {
            setNeoPixel(NEOPIXEL_RED);
            delay(1000); // linger for a moment on error
            return false;
        }
    }

    // Open file in append mode
    File dataFile = SD.open(currentFile, FILE_APPEND);
    if (!dataFile)
    {
        Serial.println("Failed to open file for logging: " + currentFile);
        setNeoPixel(NEOPIXEL_RED);
        delay(1000); // linger for a moment on error
        return false;
    }

    // Set initial NeoPixel color based on motion
    setNeoPixel(motionCount ? NEOPIXEL_GREEN : NEOPIXEL_BLUE);

    // Get date/time and sensor readings
    DateTime now = getDateTime();

    // Take forced measurement before reading BME280 values
    if (_isEnvSensorInitialized)
    {
        _envSensor.takeForcedMeasurement();
    }

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
             motionCount);

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
        delay(1000);               // linger for a moment on error
    }
    Serial.flush();

    // Add debug delay if enabled
    if (doDebug())
    {
        delay(DEBUG_DELAY_MS);
    }

    return success;
}

void HublinkBEAM::sleep(uint32_t seconds)
{
    disableNeoPixel();
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(PIN_GREEN_LED, LOW);

    // Prepare sensors for sleep (needed for both stages)
    if (_isPIRInitialized)
    {
        _pirSensor.enableTriggerMode();
    }

    if (_isBatteryMonitorInitialized)
    {
        _batteryMonitor.enableSleep(true); // Enable sleep capability
        _batteryMonitor.sleep(true);       // Enter sleep mode
    }

    // Only initialize sleep configuration if we're not already in a sleep cycle
    if (sleep_config.sleep_stage == SLEEP_STAGE_NORMAL)
    {
        // Clear any existing PIR counts before starting new cycle
        _ulp.clearPIRCount();

        sleep_config.sleep_duration = seconds;
        sleep_config.sleep_start_time = getUnixTime();
        sleep_config.sleep_stage = SLEEP_STAGE_GPIO_MONITOR;

        Serial.println("\nPreparing for Stage 1 Deep Sleep (GPIO monitoring)");
        Serial.printf("  Duration: %d seconds\n", seconds);
        Serial.printf("  Start time: %d\n", sleep_config.sleep_start_time);

        // Initialize ULP program (but don't start it yet)
        _ulp.begin();
    }
    else
    {
        Serial.println("\nContinuing existing sleep cycle");
        Serial.printf("  Stage: %d\n", sleep_config.sleep_stage);
        Serial.printf("  Original start time: %d\n", sleep_config.sleep_start_time);
    }

    Serial.println("Entering Deep Sleep");
    Serial.flush();

    // Configure GPIO wakeup on SDA_GPIO (LOW)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)SDA_GPIO, 0);

    // Calculate remaining time based on original start time
    uint32_t current_time = getUnixTime();
    uint32_t elapsed = current_time - sleep_config.sleep_start_time;
    uint32_t remaining = sleep_config.sleep_duration;
    if (elapsed < sleep_config.sleep_duration)
    {
        remaining = sleep_config.sleep_duration - elapsed;
    }

    // Enable timer wakeup for the remaining duration
    uint64_t microseconds = (uint64_t)remaining * 1000000ULL;
    esp_sleep_enable_timer_wakeup(microseconds);

    esp_deep_sleep_start();
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