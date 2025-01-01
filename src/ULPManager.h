#ifndef ULP_MANAGER_H
#define ULP_MANAGER_H

#include <Arduino.h>
#include "esp32s3/ulp.h"
#include "driver/rtc_io.h"
#include "soc/rtc_io_reg.h"

// Move constants outside the class
static const uint32_t MOTION_FLAG = 0; // RTC memory index for motion flag
static const uint32_t PROG_START = 1;  // Program start address
#define SDA_GPIO GPIO_NUM_3

class ULPManager
{
public:
    ULPManager();
    void start(); // Initialize and start the ULP program
    void stop();  // Stop the ULP program
    bool getEventFlag();
    void clearEventFlag();

private:
    bool _initialized;
};

#endif