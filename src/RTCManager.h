#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <Preferences.h>

#define PREFS_NAMESPACE "beam"
#define UPLOAD_DELAY_SECONDS 30 // Compensation for delay between compilation and upload

class RTCManager
{
public:
    RTCManager();
    bool begin();

    // Basic RTC functions
    DateTime now();
    float getTemperature();
    void serialPrintDateTime();

    // Time adjustment functions
    void adjustRTC(uint32_t timestamp);
    void adjustRTC(const DateTime &dt);

    // Compilation time management
    bool isNewCompilation();
    void updateCompilationID();

    // Helper functions
    String getDayOfWeek();
    uint32_t getUnixTime();
    DateTime getFutureTime(int days, int hours, int minutes, int seconds);

private:
    RTC_DS3231 _rtc;
    Preferences _preferences;
    bool _isInitialized;

    void updateRTC();
    String getCompileDateTime();
    DateTime getCompensatedDateTime();
    static const char *_daysOfWeek[7];

    // Default time constants
    static const uint32_t DEFAULT_TIMESTAMP = SECONDS_FROM_1970_TO_2000;
};

#endif