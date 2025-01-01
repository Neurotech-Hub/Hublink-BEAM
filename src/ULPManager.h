#ifndef ULP_MANAGER_H
#define ULP_MANAGER_H

#include <Arduino.h>
#include "esp32s3/ulp.h"
#include "driver/rtc_io.h"
#include "soc/rtc_io_reg.h"

class ULPManager
{
public:
    ULPManager();
    void start(); // Initialize and start the ULP program
    bool getEventFlag();
    void clearEventFlag();

private:
    bool _initialized;
    static const uint32_t MOTION_FLAG = 0; // RTC memory index for motion flag
    static const uint32_t PROG_START = 1;  // Program start address
};

#endif