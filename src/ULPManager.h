#ifndef ULP_MANAGER_H
#define ULP_MANAGER_H

#include <Arduino.h>
#include "esp32s3/ulp.h"
#include "driver/rtc_io.h"
#include "soc/rtc_io_reg.h"
#include "ulp_common.h"
#include "SharedDefs.h"

// Move constants outside the class
enum
{
    PIR_COUNT,          // RTC memory location for motion counter
    INACTIVITY_COUNT,   // Count of times inactivity period was exceeded
    INACTIVITY_TRACKER, // Current consecutive inactive seconds
    INACTIVITY_PERIOD,  // Target period for inactivity in seconds
    PROG_START          // Program start address
};

// ESP32-S3 specific GPIO mappings
#define SDA_GPIO GPIO_NUM_3 // GPIO3 for SDA

class ULPManager
{
public:
    ULPManager();
    void begin();
    void start(); // Initialize and start the ULP program
    void stop();  // Stop the ULP program

    // PIR count methods
    uint16_t getPIRCount();
    void clearPIRCount();

    // Inactivity tracking methods
    void setInactivityPeriod(uint16_t seconds);
    uint16_t getInactivityCount();
    uint16_t getInactivityTracker();
    void clearInactivityCounters();

private:
    bool _initialized;
};

#endif