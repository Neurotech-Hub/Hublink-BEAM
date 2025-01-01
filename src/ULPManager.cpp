#include "ULPManager.h"

// https://docs.espressif.com/projects/esp-idf/en/release-v3.3/api-guides/ulp_macros.html
// ULP program to monitor PIR trigger (GPIO3) for LOW state
const ulp_insn_t ulp_program[] = {
    // Read GPIO3 state
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If HIGH (1), branch to delay
    I_BG(3, 0), // Branch if greater than 0 (i.e., HIGH)

    // Set motion flag (R2 = memory address 0)
    I_MOVI(R2, 0),
    I_MOVI(R1, 1),
    I_ST(R1, R2, 0),

    // Delay and loop
    M_LABEL(1),
    I_DELAY(0xFFFF),
    M_BX(0) // Loop back to start
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
    rtc_gpio_init(GPIO_NUM_3);
    rtc_gpio_set_direction(GPIO_NUM_3, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(GPIO_NUM_3);
    rtc_gpio_pulldown_dis(GPIO_NUM_3);
    rtc_gpio_hold_en(GPIO_NUM_3);

    // Load ULP program
    Serial.println("      - Loading ULP program...");
    size_t size = sizeof(ulp_program) / sizeof(ulp_insn_t);
    esp_err_t err = ulp_process_macros_and_load(PROG_START, ulp_program, &size);

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