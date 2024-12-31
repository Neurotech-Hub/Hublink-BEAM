#include "RTCManager.h"

const char *RTCManager::_daysOfWeek[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

RTCManager::RTCManager() : _isInitialized(false)
{
}

bool RTCManager::begin()
{
    if (!_rtc.begin())
    {
        Serial.println("Couldn't find RTC");
        return false;
    }

    if (!_preferences.begin(PREFS_NAMESPACE, false))
    {
        Serial.println("Failed to initialize preferences");
        return false;
    }

    if (isNewCompilation())
    {
        updateRTC();
        updateCompilationID();
    }

    if (_rtc.lostPower())
    {
        Serial.println("RTC lost power, updating time from compilation");
        updateRTC();
    }

    _isInitialized = true;
    _preferences.end();
    return true;
}

DateTime RTCManager::now()
{
    return _rtc.now();
}

float RTCManager::getTemperature()
{
    return _rtc.getTemperature();
}

void RTCManager::serialPrintDateTime()
{
    DateTime current = now();
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d",
                  current.year(), current.month(), current.day(),
                  current.hour(), current.minute(), current.second());
}

void RTCManager::adjustRTC(uint32_t timestamp)
{
    Serial.println("Adjusting RTC with Unix timestamp: " + String(timestamp));
    _rtc.adjust(DateTime(timestamp));
    Serial.print("RTC time after adjustment: ");
    serialPrintDateTime();
    Serial.println();
}

void RTCManager::adjustRTC(const DateTime &dt)
{
    _rtc.adjust(dt);
}

String RTCManager::getDayOfWeek()
{
    return String(_daysOfWeek[now().dayOfTheWeek()]);
}

uint32_t RTCManager::getUnixTime()
{
    return now().unixtime();
}

DateTime RTCManager::getFutureTime(int days, int hours, int minutes, int seconds)
{
    TimeSpan span(days, hours, minutes, seconds);
    return now() + span;
}

String RTCManager::getCompileDateTime()
{
    return String(__DATE__) + " " + String(__TIME__);
}

DateTime RTCManager::getCompensatedDateTime()
{
    // Parse __DATE__ and __TIME__ strings
    char monthStr[4];
    int month, day, year, hour, minute, second;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(__DATE__, "%s %d %d", monthStr, &day, &year);
    month = (strstr(month_names, monthStr) - month_names) / 3 + 1;
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    // Create DateTime object for compilation time
    DateTime compileTime(year, month, day, hour, minute, second);

    // Add upload delay compensation
    TimeSpan uploadDelay(0, 0, 0, UPLOAD_DELAY_SECONDS);
    return compileTime + uploadDelay;
}

bool RTCManager::isNewCompilation()
{
    _preferences.begin(PREFS_NAMESPACE, false);
    const String currentCompileTime = getCompileDateTime();
    const String storedCompileTime = _preferences.getString("compileTime", "");
    _preferences.end();
    return (storedCompileTime != currentCompileTime);
}

void RTCManager::updateCompilationID()
{
    _preferences.begin(PREFS_NAMESPACE, false);
    const String currentCompileTime = getCompileDateTime();
    _preferences.putString("compileTime", currentCompileTime.c_str());
    _preferences.end();
}

void RTCManager::updateRTC()
{
    String compileDateTime = getCompileDateTime();
    Serial.println("\nRTC Time Update:");
    Serial.println("---------------");
    Serial.println("Compilation time: " + compileDateTime);

    // Get compensated DateTime
    DateTime compensatedTime = getCompensatedDateTime();

    // Format and print the compensation information
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             compensatedTime.hour(), compensatedTime.minute(), compensatedTime.second());
    Serial.println("Adding " + String(UPLOAD_DELAY_SECONDS) + " seconds compensation");
    Serial.println("Adjusted time:   " + String(timeStr));

    // Update RTC with compensated time
    _rtc.adjust(compensatedTime);

    Serial.print("Final RTC time: ");
    serialPrintDateTime();
    Serial.println("\n---------------");
}