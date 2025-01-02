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
    if (!_isInitialized)
    {
        return DateTime(DEFAULT_TIMESTAMP);
    }
    return _rtc.now();
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
    _rtc.adjust(DateTime(timestamp));
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
    if (!_isInitialized)
    {
        return DateTime(DEFAULT_TIMESTAMP);
    }
    TimeSpan span(days, hours, minutes, seconds);
    return now() + span;
}

String RTCManager::getCompileDateTime()
{
    const char *date = __DATE__;
    const char *time = __TIME__;
    char compileDateTime[20];
    // Convert compile date/time to string
    snprintf(compileDateTime, sizeof(compileDateTime), "%s %s", date, time);
    return String(compileDateTime);
}

DateTime RTCManager::getCompensatedDateTime()
{
    // Parse __DATE__ and __TIME__ strings
    char monthStr[4];
    int month, day, year, hour, minute, second;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    // Parse date string
    sscanf(__DATE__, "%s %d %d", monthStr, &day, &year);
    month = (strstr(month_names, monthStr) - month_names) / 3 + 1;

    // Parse time string
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    // Create DateTime object for compilation time
    DateTime compileTime(year, month, day, hour, minute, second);

    // Add upload delay compensation
    return compileTime + TimeSpan(0, 0, 0, UPLOAD_DELAY_SECONDS);
}

bool RTCManager::isNewCompilation()
{
    _preferences.begin(PREFS_NAMESPACE, false);
    const String currentCompileTime = getCompileDateTime();
    String storedCompileTime = _preferences.getString("compileTime", "");

    Serial.println("\nChecking compilation status:");
    Serial.println("---------------------------");
    Serial.println("Current compile time: " + currentCompileTime);
    Serial.println("Stored compile time:  " + storedCompileTime);
    Serial.println("Is new compilation:   " + String(currentCompileTime != storedCompileTime));
    Serial.println("---------------------------\n");

    _preferences.end();
    return currentCompileTime != storedCompileTime;
}

void RTCManager::updateCompilationID()
{
    _preferences.begin(PREFS_NAMESPACE, false);
    const String currentCompileTime = getCompileDateTime();

    Serial.println("\nUpdating compilation ID:");
    Serial.println("----------------------");
    Serial.println("Storing new compile time: " + currentCompileTime);

    _preferences.putString("compileTime", currentCompileTime.c_str());

    // Verify storage
    String verifyTime = _preferences.getString("compileTime", "");
    Serial.println("Verified stored time:    " + verifyTime);
    Serial.println("Storage successful:      " + String(verifyTime == currentCompileTime));
    Serial.println("----------------------\n");

    _preferences.end();
}

void RTCManager::updateRTC()
{
    String compileDateTime = getCompileDateTime();

    // Get compensated DateTime
    DateTime compensatedTime = getCompensatedDateTime();

    // Format and print the compensation information
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             compensatedTime.year(), compensatedTime.month(), compensatedTime.day(),
             compensatedTime.hour(), compensatedTime.minute(), compensatedTime.second());

    // Update RTC with compensated time
    _rtc.adjust(compensatedTime);

    // Verify the time was set correctly
    DateTime currentTime = _rtc.now();
    char currentTimeStr[20];
    snprintf(currentTimeStr, sizeof(currentTimeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             currentTime.year(), currentTime.month(), currentTime.day(),
             currentTime.hour(), currentTime.minute(), currentTime.second());
}