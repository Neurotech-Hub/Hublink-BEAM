#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

// Preferences namespace (shared across library)
#define PREFS_NAMESPACE "beam"

// Sleep stages
enum SleepStage
{
    SLEEP_STAGE_NORMAL = 0,
    SLEEP_STAGE_GPIO_MONITOR = 1,
    SLEEP_STAGE_ULP_MONITOR = 2
};

// RTC Memory locations (must be after ULP program memory)
enum RTCMemorySlots
{
    RTC_SLEEP_CONFIG = 2 // After PIR_COUNT(0) and PROG_START(1)
};

// Sleep configuration structure (stored in RTC memory)
typedef struct
{
    uint32_t sleep_duration;   // Original sleep duration in seconds
    uint32_t sleep_start_time; // When we started sleeping (Unix timestamp)
    uint32_t alarm_start_time; // When we started counting for alarm (Unix timestamp)
    uint8_t sleep_stage;       // Uses SleepStage enum
    uint16_t alarm_interval;   // Alarm interval in minutes (0 = disabled)
} sleep_config_t;

#endif