#include "ULPManager.h"

// https://docs.espressif.com/projects/esp-idf/en/release-v3.3/api-guides/ulp_macros.html
// ULP program to monitor PIR trigger (GPIO3) for LOW state
const ulp_insn_t ulp_program[] = {
    I_MOVI(R2, MOTION_FLAG), // set R2 to motion flag address

    M_LABEL(0),
    I_DELAY(0xFFFF), // delay for branch loop

    // Read GPIO3 state into R0 (processor definitions do work on R0)
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),
    M_BL(0, 1), // (label, value): If R0 < value, branch to label 0 else continue

    I_ST(R0, R2, 0), // (reg_val, reg_addr, offset_): store R0 (GPIO state) into motion flag
    I_HALT(),        // Halt the ULP program
};

ULPManager::ULPManager()
{
    _initialized = false;
}

void ULPManager::start()
{
    Serial.println("      - Initializing ULP...");

    // Clear RTC memory
    memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);

    // Configure GPIO3 for ULP
    Serial.println("      - Configuring GPIO3 for ULP...");
    rtc_gpio_init(SDA_GPIO);
    rtc_gpio_set_direction(SDA_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(SDA_GPIO);
    rtc_gpio_pulldown_dis(SDA_GPIO);
    rtc_gpio_hold_en(SDA_GPIO);

    // Load ULP program
    Serial.println("      - Loading ULP program...");
    size_t size = sizeof(ulp_program) / sizeof(ulp_insn_t);
    esp_err_t err = ulp_process_macros_and_load(PROG_START, ulp_program, &size);

    return;

    if (err != ESP_OK)
    {
        Serial.printf("      - ULP program load error: %d\n", err);
        return;
    }

    // Start ULP program
    Serial.println("      - Starting ULP program...");
    err = ulp_run(PROG_START);
    if (err != ESP_OK)
    {
        Serial.printf("      - Error starting ULP program: %d\n", err);
        return;
    }

    _initialized = true;
    Serial.println("      - ULP initialization complete");
}

void ULPManager::stop()
{
    rtc_gpio_hold_dis(SDA_GPIO); // Release the hold on GPIO3
    delay(10);                   // Give some time for the pin to stabilize
}

bool ULPManager::getEventFlag()
{
    if (!_initialized)
        return false;
    return RTC_SLOW_MEM[MOTION_FLAG] != 0;
}

void ULPManager::clearEventFlag()
{
    if (!_initialized)
        return;
    RTC_SLOW_MEM[MOTION_FLAG] = 0;
}