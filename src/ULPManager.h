#ifndef ULP_MANAGER_H
#define ULP_MANAGER_H

#include <Arduino.h>
#include "esp32s3/ulp.h"
#include "driver/rtc_io.h"
#include "soc/rtc_io_reg.h"

// Move constants outside the class
enum
{
    PIR_COUNT, // RTC memory location for motion counter
    PROG_START // Program start address
};

// ESP32-S3 specific GPIO mappings
#define SDA_GPIO GPIO_NUM_3 // GPIO3 for SDA

class ULPManager
{
public:
    ULPManager();
    void start(); // Initialize and start the ULP program
    void stop();  // Stop the ULP program
    uint16_t getPIRCount();
    void clearPIRCount();

private:
    bool _initialized;
};

#endif