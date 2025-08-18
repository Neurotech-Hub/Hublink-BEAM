#include "HublinkBEAM.h"
#include "RTCManager.h"
#include "esp_sleep.h"
#include "esp_mac.h"

// Use RTC memory to maintain state across deep sleep
static RTC_DATA_ATTR uint16_t alarm_interval = 0;
static RTC_DATA_ATTR uint32_t alarm_start_time = 0;
static RTC_DATA_ATTR uint32_t sleep_start_time = 0;

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
    _pir_percent_active = 0.0;
}

void HublinkBEAM::setDeviceID(String deviceID)
{
    // Validate device ID: must be exactly 3 characters and alphanumeric
    if (deviceID.length() != 3)
    {
        Serial.println("Warning: Device ID must be exactly 3 characters, using default 'XXX'");
        _deviceID = "XXX";
        return;
    }

    // Check if all characters are alphanumeric
    bool isValid = true;
    for (int i = 0; i < 3; i++)
    {
        char c = deviceID.charAt(i);
        if (!isalnum(c))
        {
            isValid = false;
            break;
        }
    }

    if (!isValid)
    {
        Serial.println("Warning: Device ID must be alphanumeric, using default 'XXX'");
        _deviceID = "XXX";
        return;
    }

    // Convert to uppercase for consistency
    deviceID.toUpperCase();
    _deviceID = deviceID;
    Serial.printf("Device ID set to: %s\n", _deviceID.c_str());
}

bool HublinkBEAM::begin()
{
    // Stop ULP to free up GPIO pins and stop ULP timer
    _ulp.stop();

    // Set CPU frequency to 80MHz
    setCpuFrequencyMhz(80);

    // Initialize pins and set NeoPixel to blue during initialization
    initPins();
    setNeoPixel(NEOPIXEL_BLUE);

    Serial.begin(115200);
    if (switchADown())
    {
        // Wait up to 10 seconds for Serial connection
        unsigned long startTime = millis();
        while (!Serial && (millis() - startTime < 5000))
        {
            delay(100); // Small delay to prevent tight loop
        }
        Serial.println("***Debug mode enabled***");
    }
    // Normal initialization for timer wakeup or regular boot
    Serial.println("\n\n\n----------\nbeam.begin()...\n----------\n");

    // Initialize I2C for all cases
    Wire.begin();
    delay(10); // Give I2C time to stabilize
    Serial.println("  I2C: started");

    // Calculate PIR activity percentage if waking from sleep
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    _isWakeFromSleep = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
    Serial.printf("    Wake from sleep: %s\n", _isWakeFromSleep ? "YES" : "NO");

    bool allInitialized = true; // Assume everything is OK until proven otherwise

    // Always reinitialize SD card after deep sleep
    if (!initSD())
    {
        Serial.println("*** SD card initialization failed in begin() ***");
        allInitialized = false;
    }

    // Initialize sensors (with optimization if waking from sleep)
    if (!initSensors(_isWakeFromSleep))
    {
        allInitialized = false;
    }

    // Only print full initialization report on first boot
    if (!_isWakeFromSleep)
    {
        digitalWrite(PIN_FRONT_LED, HIGH);
        _ulp.clearPIRCount();
        _ulp.clearInactivityCounters();
        _pir_percent_active = 0.0;
        _inactivity_fraction = 0.0;
        alarm_interval = 0;
        alarm_start_time = 0;

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
    else // waking from deep sleep
    {
        // Calculate and store time values
        _elapsed_seconds = getUnixTime() - sleep_start_time;
        _active_seconds = static_cast<double>(_ulp.getPIRCount()) * 1.0;

        // Calculate activity percentage
        _pir_percent_active = (_elapsed_seconds > 0) ? std::min(1.0, _active_seconds / static_cast<double>(_elapsed_seconds)) : 0.0;

        /*
        Timeline Example (elapsed_seconds = 100, _inactivityPeriod = 20)
        |----|----|----|----|----| Each "|" represents 20 seconds (inactivityPeriod)
        0    20   40   60   80   100

        Case 1: Some motion detected (pir_count = 3)
        active_seconds = 3
        X = motion detected
        |-X--|--X-|----|-X--|----| (inactivity_count = 2 complete periods without motion)
            ^         ^    ^
            motion    motion motion

        inactive_seconds = inactivity_count * _inactivityPeriod = 2 * 20 = 40
        _inactivity_fraction = 40 / (40 + 3) ≈ 0.93 (93% inactive)

        Case 2: Frequent motion (pir_count = 15)
        X = motion detected
        |XX--|XXXX|X-X-|XX-X|X--X|
        inactive_seconds = inactivity_count * _inactivityPeriod = 0 * 20 = 0
        _inactivity_fraction = 0 / (0 + 15) = 0 (0% inactive)

        Case 3: No motion (pir_count = 0)
        |----|----|----|----|----| (inactivity_count = 5)
        inactive_seconds = inactivity_count * _inactivityPeriod = 5 * 20 = 100
        _inactivity_fraction = 100 / (100 + 0) = 1.0 (100% inactive)
        */

        Serial.printf("\nActivity Report:\n");
        Serial.printf("  Total time: %d seconds\n", _elapsed_seconds);
        Serial.printf("  Active time: %.3f seconds\n", _active_seconds);
        Serial.printf("  Activity percentage: %.3f%%\n", _pir_percent_active * 100.0);
    }

    // Set final NeoPixel state based on initialization result
    if (allInitialized)
    {
        disableNeoPixel(); // Turn off if everything is OK
        digitalWrite(PIN_FRONT_LED, LOW);
    }
    else
    {
        Serial.println("*** INITIALIZATION FAILED ***");
        Serial.printf("  SD: %s\n", _isSDInitialized ? "OK" : "FAILED");
        Serial.printf("  PIR: %s\n", _isPIRInitialized ? "OK" : "FAILED");
        Serial.printf("  Battery: %s\n", _isBatteryMonitorInitialized ? "OK" : "FAILED");
        Serial.printf("  Env: %s\n", _isEnvSensorInitialized ? "OK" : "FAILED");
        Serial.printf("  Light: %s\n", _isLightSensorInitialized ? "OK" : "FAILED");
        Serial.printf("  RTC: %s\n", _isRTCInitialized ? "OK" : "FAILED");
        Serial.printf("  Low Battery: %s\n", _isLowBattery ? "YES" : "NO");

        if (_isLowBattery)
        {
            setNeoPixel(NEOPIXEL_PURPLE); // Purple for low battery
        }
        else
        {
            setNeoPixel(NEOPIXEL_RED); // Red for other failures
        }
    }

    // Apply random delay if randomization is enabled and initialization succeeded
    if (allInitialized && _alarmRandomizationMinutes > 0)
    {
        uint32_t hash = hashMacAddress();
        uint16_t randomDelaySeconds = hash % (2 * _alarmRandomizationMinutes * 60 + 1);
        Serial.printf("\nApplying random delay: %d seconds (%.1f minutes) based on MAC address\n",
                      randomDelaySeconds, randomDelaySeconds / 60.0);

        // Skip delay in debug mode (Switch A down) for faster development
        if (!switchADown())
        {
            delay(randomDelaySeconds * 1000);
        }
        else
        {
            Serial.println("Debug mode: skipping random delay");
        }
    }

    Serial.flush();

    return allInitialized;
}

void HublinkBEAM::initPins()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Initialize SD card pins
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);     // Deselect SD card by default
    pinMode(PIN_SD_DET, INPUT_PULLUP); // converts to input during sleep
    pinMode(PIN_SD_PWR_EN, OUTPUT);
    enableSDPower();

    // Add delay for SD card power stabilization
    delay(50); // Give SD card time to power up and stabilize

    // Ensure SPI pins are configured properly (especially after wake from sleep)
    pinMode(MOSI, OUTPUT);
    pinMode(MISO, INPUT);
    pinMode(SCK, OUTPUT);

    // Initialize on-board LED
    pinMode(PIN_FRONT_LED, OUTPUT);
    digitalWrite(PIN_FRONT_LED, LOW); // Turn off green LED

    // on board GPIO
    pinMode(PIN_SWITCH_A, INPUT_PULLUP);
    pinMode(PIN_SWITCH_B, INPUT_PULLUP);
    pinMode(TP_1, INPUT_PULLUP);
    pinMode(TP_2, INPUT_PULLUP);

    // Initialize NeoPixel power pin and pixel
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
    _pixel.begin();
    _pixel.setBrightness(NEOPIXEL_DIM); // Set to low brightness
    _pixel.show();                      // Initialize all pixels to 'off'

    // turn off I2C power for Adafruit ESP32-S3 Feather (uses hardware pulldown)
    pinMode(PIN_I2C_POWER, INPUT);
}

bool HublinkBEAM::initSensors(bool isWakeFromSleep)
{
    bool allInitialized = true;
    Serial.println("Initializing sensors...");

    // Initialize battery monitor with detailed debug
    if (!_batteryMonitor.begin(&Wire))
    {
        Serial.println("  Battery: failed to begin()");
        allInitialized = false;
        _isBatteryMonitorInitialized = false;
    }
    else
    {
        Serial.println("  Battery: begin() OK");
        _isBatteryMonitorInitialized = true;

        // Replace single delay with retry loop
        const uint8_t MAX_RETRIES = 20;
        uint8_t retries = 0;
        float voltage = 0;

        while (retries < MAX_RETRIES)
        {
            delay(10); // Shorter delay between attempts
            voltage = _batteryMonitor.cellVoltage();
            Serial.printf("  Battery init - attempt %d: %.2fV\n", retries + 1, voltage);

            if (voltage > 0 && !isnan(voltage))
            {
                break; // Valid reading obtained
            }
            retries++;
        }

        // Continue with existing battery check logic
        float percent = _batteryMonitor.cellPercent();
        Serial.printf("  Battery debug - Final values:\n");
        Serial.printf("    Voltage: %.2fV\n", voltage);
        Serial.printf("    Percent: %.1f%%\n", percent);
        Serial.printf("    isnan(voltage): %d\n", isnan(voltage));

        // Allow to run on low battery if switch A is down
        if (voltage < LOW_BATTERY_THRESHOLD && !switchADown())
        {
            Serial.printf("  Low battery detected: %.2fV\n", voltage);
            _isLowBattery = true;
            return false;
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
            // Use delay if USB is connected, light sleep has issues disconnecting otherwise
            int delayTime = switchADown() ? 3000 : ZDP323_TSTAB_MS;
            if (Serial)
            {
                Serial.println("  PIR: using delay (USB connected)");
                setNeoPixel(NEOPIXEL_PURPLE);
                delay(delayTime);
            }
            else
            {
                Serial.println("  PIR: using light sleep");
                esp_sleep_enable_timer_wakeup(delayTime * 1000); // Convert ms to microseconds
                esp_light_sleep_start();
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            }
        }
        _isPIRInitialized = true;
    }

    Serial.printf("  All sensors %s\n", allInitialized ? "OK" : "FAILED");
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

    // Try SD initialization with retry logic
    const uint8_t MAX_RETRIES = 3;
    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++)
    {
        if (attempt > 0)
        {
            Serial.printf("SD init retry %d/%d\n", attempt + 1, MAX_RETRIES);
            delay(100); // Brief delay between retries
        }

        if (SD.begin(PIN_SD_CS))
        {
            SD.exists("/x.txt"); // trick to enter SD idle state
            _isSDInitialized = true;
            Serial.println("SD card initialized successfully");
            return true;
        }
    }

    Serial.println("SD card initialization failed after all retries!");
    return false;
}

bool HublinkBEAM::isSDCardPresent()
{
    return !digitalRead(PIN_SD_DET); // Pin is pulled up, so LOW means card is present
}

void HublinkBEAM::enableSDPower()
{
    digitalWrite(PIN_SD_PWR_EN, HIGH);
}

void HublinkBEAM::disableSDPower()
{
    digitalWrite(PIN_SD_PWR_EN, LOW);
}

bool HublinkBEAM::switchADown()
{
    return digitalRead(PIN_SWITCH_A) == LOW;
}

bool HublinkBEAM::switchBDown()
{
    return digitalRead(PIN_SWITCH_B) == LOW;
}

String HublinkBEAM::getCurrentFilename()
{
    DateTime now = getDateTime();
    Serial.println("\nGetting current filename...");
    Serial.printf("  Wake from sleep: %s\n", _isWakeFromSleep ? "YES" : "NO");
    Serial.printf("  newFileOnBoot: %s\n", getNewFileOnBoot() ? "YES" : "NO");

    // First check stored filename from preferences
    _preferences.begin(PREFS_NAMESPACE, false);
    String storedFilename = _preferences.getString("filename", "");
    _preferences.end();

    // If we have a stored filename, check if it's from today
    if (storedFilename.length() > 0)
    {
        // Extract date from stored filename (format: /BEAMXXX_YYYYMMDDXX.csv)
        // Find the underscore after device ID to locate the date part
        int underscorePos = storedFilename.indexOf('_');
        if (underscorePos != -1)
        {
            int dateStart = underscorePos + 1;
            int storedYear = storedFilename.substring(dateStart, dateStart + 4).toInt();
            int storedMonth = storedFilename.substring(dateStart + 4, dateStart + 6).toInt();
            int storedDay = storedFilename.substring(dateStart + 6, dateStart + 8).toInt();

            Serial.println("\nComparing dates:");
            Serial.printf("  Stored filename: %s\n", storedFilename.c_str());
            Serial.printf("  Stored date: %04d-%02d-%02d\n", storedYear, storedMonth, storedDay);
            Serial.printf("  Current date: %04d-%02d-%02d\n", now.year(), now.month(), now.day());

            bool isFromToday = (storedYear == now.year() &&
                                storedMonth == now.month() &&
                                storedDay == now.day());

            // If waking from sleep and file is from today, use it
            if (_isWakeFromSleep && isFromToday && SD.exists(storedFilename))
            {
                Serial.printf("  Using existing file (wake from sleep): %s\n", storedFilename.c_str());
                return storedFilename;
            }

            // If not waking from sleep and newFileOnBoot is false, try to use today's file
            if (!_isWakeFromSleep && !getNewFileOnBoot() && isFromToday && SD.exists(storedFilename))
            {
                Serial.printf("  Using existing file (same day): %s\n", storedFilename.c_str());
                return storedFilename;
            }
        }
        else
        {
            Serial.println("  Invalid stored filename format (no underscore found)");
        }
    }

    // If we get here, we need to either:
    // 1. Find first existing file from today (if !newFileOnBoot)
    // 2. Create a new file (if newFileOnBoot or no existing file found)

    // Try to find existing file from today if newFileOnBoot is false
    if (!getNewFileOnBoot())
    {
        char baseFilename[11]; // YYYYMMDD (8 chars + null terminator)
        snprintf(baseFilename, sizeof(baseFilename), "%04d%02d%02d",
                 now.year(), now.month(), now.day());

        Serial.println("  Looking for existing files from today:");
        // Try each possible number (00-99)
        for (uint8_t num = 0; num < 100; num++)
        {
            char testFilename[28]; // /BEAMXXX_YYYYMMDDXX.csv (23 chars + null terminator)
            snprintf(testFilename, sizeof(testFilename), "/BEAM%s_%s%02d.csv",
                     _deviceID.c_str(), baseFilename, num);
            Serial.printf("    Testing: %s - ", testFilename);

            if (SD.exists(testFilename))
            {
                Serial.println("exists, using this file");
                _preferences.begin(PREFS_NAMESPACE, false);
                _preferences.putString("filename", String(testFilename));
                _preferences.end();
                return String(testFilename);
            }
            Serial.println("not found");
        }
        Serial.println("  No existing files found from today");
    }

    // Create new file with next available number
    char baseFilename[11];
    snprintf(baseFilename, sizeof(baseFilename), "%04d%02d%02d",
             now.year(), now.month(), now.day());
    Serial.printf("  Base filename: %s\n", baseFilename);

    Serial.println("  Scanning for next available number:");
    uint8_t nextNum = 0;
    while (nextNum < 100)
    {
        char testFilename[28]; // /BEAMXXX_YYYYMMDDXX.csv (23 chars + null terminator)
        snprintf(testFilename, sizeof(testFilename), "/BEAM%s_%s%02d.csv",
                 _deviceID.c_str(), baseFilename, nextNum);
        Serial.printf("    Testing: %s - ", testFilename);

        if (!SD.exists(testFilename))
        {
            Serial.println("available");
            _preferences.begin(PREFS_NAMESPACE, false);
            _preferences.putString("filename", String(testFilename));
            _preferences.end();
            Serial.println("  Stored in preferences");
            return String(testFilename);
        }
        Serial.println("exists");
        nextNum++;
    }

    Serial.println("Error: No available file numbers!");
    return String();
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
    if (switchADown())
    {
        delay(2000);
    }

    uint16_t pirCount = _ulp.getPIRCount(); // clear in sleep()
    uint16_t inactivityCount = (_inactivityPeriod > 0) ? _ulp.getInactivityCount() : 0;
    _minFreeHeap = ESP.getMinFreeHeap();

    // Check for required sensors and SD card
    if (!_isSDInitialized)
    {
        Serial.println("Cannot log: SD card not initialized");
        setNeoPixel(NEOPIXEL_RED);
        return false;
    }

    if (!isSDCardPresent())
    {
        Serial.println("Cannot log: SD card not present");
        setNeoPixel(NEOPIXEL_RED);
        return false;
    }

    // Test SD card is actually working by attempting to read card info
    if (!SD.cardSize())
    {
        Serial.println("Cannot log: SD card not responding (cardSize failed)");
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
        Serial.println("*** SD card may have failed after initialization ***");
        // Try to check if SD card is still present and working
        if (!isSDCardPresent())
        {
            Serial.println("SD card no longer present!");
        }
        else if (!SD.cardSize())
        {
            Serial.println("SD card no longer responding!");
        }
        setNeoPixel(NEOPIXEL_RED);
        delay(1000); // linger for a moment on error
        return false;
    }

    // Set initial NeoPixel color based on motion
    setNeoPixel(pirCount ? NEOPIXEL_GREEN : NEOPIXEL_BLUE);

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

    // Calculate inactivity fraction if period is set and we're waking from sleep
    _inactivity_fraction = 0.0; // Default for non-wake or no period set
    if (_isWakeFromSleep && _inactivityPeriod > 0)
    {
        const double possible_inactive_periods = static_cast<double>(_elapsed_seconds) /
                                                 static_cast<double>(_inactivityPeriod);
        if (possible_inactive_periods > 0)
        {
            const double inactive_seconds = static_cast<double>(inactivityCount) *
                                            static_cast<double>(_inactivityPeriod);
            // note, inactive seconds is tallied based on _inactivityPeriod, not actual single seconds.
            // So by including active_seconds in the denominator, we are able to calculate the fraction of time
            // that was inactive to include the time outside of the last _inactivityPeriod; even a full
            // _inactivityPeriod wasn't met, we still have a true fraction of time that was inactive.
            _inactivity_fraction = std::min(1.0,
                                            inactive_seconds / (inactive_seconds + _active_seconds));
        }
    }

    if (_inactivityPeriod > 0)
    {
        Serial.printf("  Inactivity period: %d seconds\n", _inactivityPeriod);
        Serial.printf("  Inactivity fraction: %.3f%%\n", _inactivity_fraction * 100.0);
    }

    // Format data string with new fields
    char dataString[128];
    snprintf(dataString, sizeof(dataString),
             "%04d-%02d-%02d %02d:%02d:%02d,%lu,%s,%s,%.3f,%.2f,%.2f,%.2f,%.4f,%d,%.3f,%d,%d,%.3f,%lu,%d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second(),
             millis(),
             _deviceID.c_str(),
             HUBLINK_BEAM_VERSION,
             batteryV,
             tempC,
             pressHpa,
             humidity,
             lux,
             pirCount,
             _pir_percent_active,
             _inactivityPeriod,
             inactivityCount,
             _inactivity_fraction,
             _minFreeHeap,
             !_isWakeFromSleep);

    // Write data
    bool success = dataFile.println(dataString);
    dataFile.close();

    // Print formatted values using the same variables
    char datetime[20];
    snprintf(datetime, sizeof(datetime), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    Serial.println("\n" + currentFile + ":");
    Serial.println("DateTime:    " + String(datetime));
    Serial.println("Millis:      " + String(millis()));
    Serial.println("Device ID:   " + _deviceID);
    Serial.println("Version:     " + String(HUBLINK_BEAM_VERSION));
    Serial.println("Battery V:   " + String(batteryV));
    Serial.println("Temp °C:     " + String(tempC));
    Serial.println("Press hPa:   " + String(pressHpa));
    Serial.println("Humidity %:  " + String(humidity));
    Serial.println("Light lux:   " + String(lux));
    Serial.println("PIR Count:   " + String(pirCount));
    Serial.println("PIR Active:  " + String(_pir_percent_active));
    Serial.println("Inact Sec:   " + String(_inactivityPeriod));
    Serial.println("Inact Count: " + String(inactivityCount));
    Serial.println("Inact Frac:  " + String(_inactivity_fraction));
    Serial.println("Min Heap:    " + String(_minFreeHeap));
    Serial.println("Is Reboot:   " + String(!_isWakeFromSleep));

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

    return success;
}

void HublinkBEAM::sleep(uint32_t minutes)
{
    uint32_t seconds = minutes * 60; // Convert minutes to seconds

    // Record sleep start time for PIR activity calculation
    if (_isRTCInitialized)
    {
        sleep_start_time = getUnixTime();
        Serial.printf("Recording sleep start time: %d\n", sleep_start_time);
    }

    // Configure ULP inactivity period if set
    if (_inactivityPeriod > 0)
    {
        _ulp.setInactivityPeriod(_inactivityPeriod);
    }

    // Prepare for sleep
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(PIN_FRONT_LED, LOW);
    pinMode(PIN_SD_DET, INPUT);
    SD.end();                  // 1: end SD
    disableSDPower();          // 2: disable SD power
    pinMode(PIN_SD_CS, INPUT); // 3: avoid current leakage
    pinMode(MOSI, INPUT);
    pinMode(MISO, INPUT);
    pinMode(SCK, INPUT);

    // Prepare sensors for sleep
    if (_isPIRInitialized)
    {
        _pirSensor.enableTriggerMode();
    }

    if (_isBatteryMonitorInitialized)
    {
        _batteryMonitor.enableSleep(true); // Enable sleep capability
        _batteryMonitor.sleep(true);       // Enter sleep mode
    }

    Serial.printf("Entering deep sleep for %d minutes (%d seconds)\n", minutes, seconds);
    Serial.flush();
    disableNeoPixel();

    // Enable timer wakeup
    uint64_t microseconds = (uint64_t)seconds * 1000000ULL;
    esp_sleep_enable_timer_wakeup(microseconds);
    _ulp.begin(); // configure pins
    _ulp.start(); // load/start ULP program
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

bool HublinkBEAM::alarm(uint16_t minutes)
{
    if (!_isRTCInitialized)
    {
        Serial.println("alarm: RTC not initialized");
        return false;
    }

    uint32_t current_time = getUnixTime();

    // If minutes > 0, we're potentially setting up the alarm
    if (minutes > 0)
    {
        // Only update interval and start time if this is first setup
        if (alarm_start_time == 0)
        {
            alarm_interval = minutes;
            alarm_start_time = current_time;
            Serial.printf("alarm: First setup - interval %d minutes, start time %d\n",
                          minutes, current_time);
            return false;
        }
        // Otherwise just update the interval if it changed
        else if (alarm_interval != minutes)
        {
            alarm_interval = minutes;
            Serial.printf("alarm: Updated interval to %d minutes\n", minutes);
        }
    }

    // If no interval is set, we can't check the alarm
    if (alarm_interval == 0)
    {
        Serial.println("alarm: No interval set");
        return false;
    }

    // Check if enough time has elapsed since the last alarm
    uint32_t interval_seconds = (uint32_t)alarm_interval * 60;
    uint32_t next_alarm = alarm_start_time + interval_seconds;

    Serial.println("\nChecking alarm condition:");
    Serial.printf("  Current time: %d\n", current_time);
    Serial.printf("  Next alarm: %d\n", next_alarm);
    Serial.printf("  Time to alarm: %d seconds\n",
                  (current_time < next_alarm) ? (next_alarm - current_time) : 0);

    if (current_time >= next_alarm)
    {
        // Update start time to the next interval
        alarm_start_time = next_alarm;
        Serial.println("  → Alarm triggered!");
        // If next_alarm is not in the future the RTC must have been adjusted
        // so we need to reset the alarm_start_time and next_alarm
        if (next_alarm <= current_time)
        {
            Serial.println("  Time adjustment detected, resetting alarm");
            alarm_start_time = current_time;
            next_alarm = alarm_start_time + interval_seconds;
        }
        return true;
    }

    Serial.println("  → Not triggered");
    return false;
}

uint32_t HublinkBEAM::hashMacAddress()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT); // Get Bluetooth MAC address

    // Print MAC address for debugging
    Serial.printf("Device MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Simple hash combining all 6 bytes
    uint32_t hash = 0;
    for (int i = 0; i < 6; i++)
    {
        hash = hash * 31 + mac[i];
    }

    return hash;
}