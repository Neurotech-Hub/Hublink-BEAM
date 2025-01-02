#include "ULPManager.h"

// https://docs.espressif.com/projects/esp-idf/en/release-v3.3/api-guides/ulp_macros.html
// ULP program to monitor PIR trigger (GPIO3) for LOW state
const ulp_insn_t ulp_program[] = {
    I_MOVI(R2, MOTION_FLAG), // set R2 to motion flag address

    M_LABEL(1),   // Main loop label
    I_DELAY(200), // ~11.4µs delay @ 17.5MHz (57ns per cycle)

    // Read GPIO3 state into R0
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If GPIO is LOW (motion detected), store state and halt
    M_BL(2, 1), // (label_num, imm_value); if R0 < 1 (LOW), branch to store/halt

    // Otherwise loop back to start
    M_BX(1), // Branch back to delay

    M_LABEL(2),      // Store and halt label
    I_MOVI(R0, 1),   // Load 1 into R0
    I_ST(R0, R2, 0), // Store 1 into motion flag
    I_HALT(),        // Halt until next timer wake
};

ULPManager::ULPManager()
{
    _initialized = false;
}

void ULPManager::start()
{
    // Clear RTC memory
    memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);

    // Configure I2C power pin
    rtc_gpio_init((gpio_num_t)PIN_I2C_POWER);
    rtc_gpio_set_direction((gpio_num_t)PIN_I2C_POWER, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level((gpio_num_t)PIN_I2C_POWER, 1);
    rtc_gpio_hold_en((gpio_num_t)PIN_I2C_POWER);

    // Only configure SDA (GPIO3) for ULP reading
    rtc_gpio_init(SDA_GPIO);
    rtc_gpio_set_direction(SDA_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(SDA_GPIO); // Use hardware pullup
    rtc_gpio_pulldown_dis(SDA_GPIO);
    rtc_gpio_hold_en(SDA_GPIO);

    // Load ULP program
    size_t size = sizeof(ulp_program) / sizeof(ulp_insn_t);
    esp_err_t err = ulp_process_macros_and_load(PROG_START, ulp_program, &size);

    if (err != ESP_OK)
    {
        Serial.printf("***ULP program load error: %d***\n", err);
        return;
    }

    err = ulp_run(PROG_START);
    if (err != ESP_OK)
    {
        Serial.printf("***Error starting ULP program: %d***\n", err);
        return;
    }

    _initialized = true;
}

void ULPManager::stop()
{
    // First disable holds
    rtc_gpio_hold_dis((gpio_num_t)PIN_I2C_POWER);
    rtc_gpio_hold_dis(SDA_GPIO);

    // Reset SDA pin configuration
    rtc_gpio_set_direction(SDA_GPIO, RTC_GPIO_MODE_DISABLED);
    rtc_gpio_pullup_dis(SDA_GPIO);
    rtc_gpio_pulldown_dis(SDA_GPIO);
    rtc_gpio_deinit(SDA_GPIO);

    // Reset I2C power pin configuration
    rtc_gpio_set_direction((gpio_num_t)PIN_I2C_POWER, RTC_GPIO_MODE_DISABLED);
    rtc_gpio_deinit((gpio_num_t)PIN_I2C_POWER);

    delay(10); // Give some time for the pin to stabilize
}

bool ULPManager::getEventFlag()
{
    // Access the specific 16-bit value within the 32-bit word
    uint16_t flag = (RTC_SLOW_MEM[MOTION_FLAG] & 0xFFFF);
    return flag != 0;
}

// not needed after boot since we wipe the memory in start()
void ULPManager::clearEventFlag()
{
    // Clear only the lower 16 bits
    RTC_SLOW_MEM[MOTION_FLAG] &= 0xFFFF0000;
}