#include "ULPManager.h"

// ULP program to monitor PIR trigger (GPIO3) for LOW state with optimized delay
const ulp_insn_t ulp_program[] = {
    I_MOVI(R2, PIR_COUNT), // set R2 to motion flag address
    I_LD(R3, R2, 0),       // Load current motion flag value into R3

    M_LABEL(1),
    // I_WR_REG(RTC_GPIO_OUT_REG, 13 + RTC_GPIO_OUT_DATA_S, 13 + RTC_GPIO_OUT_DATA_S, 1), // LED ON
    I_DELAY(438), // debounce 25µs

    // Read GPIO3 state into R0
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If GPIO is not LOW (no motion), skip increment
    M_BG(1, 0),        // If R0 > 0 (HIGH), branch 1 to repeat
    I_ADDI(R3, R3, 1), // Increment motion counter if LOW
    I_ST(R3, R2, 0),   // Store counter value

    // I_WR_REG(RTC_GPIO_OUT_REG, 13 + RTC_GPIO_OUT_DATA_S, 13 + RTC_GPIO_OUT_DATA_S, 0), // LED OFF

    I_MOVI(R1, 270),   // Load iteration count into R1, 270 x 0xFFFF = 1s
    M_LABEL(2),        // Start of delay loop
    I_DELAY(0xFFFF),   // Maximum delay
    I_SUBI(R1, R1, 1), // Decrement counter in R1
    I_MOVR(R0, R1),    // Move R1 to R0 for comparison
    M_BG(2, 0),        // If R0 > 0 (R1 > 0), branch to label 2
    M_BX(1),           // Jump back to main loop (label 1)
};

ULPManager::ULPManager()
{
    _initialized = false;
}

void ULPManager::begin()
{
    Serial.println("  ULP: begin");
    // Configure I2C power pin
    rtc_gpio_init((gpio_num_t)PIN_I2C_POWER);
    rtc_gpio_set_direction((gpio_num_t)PIN_I2C_POWER, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level((gpio_num_t)PIN_I2C_POWER, 1);
    rtc_gpio_hold_en((gpio_num_t)PIN_I2C_POWER);

    // Configure LED pin
    rtc_gpio_init((gpio_num_t)LED_BUILTIN);
    rtc_gpio_set_direction((gpio_num_t)LED_BUILTIN, RTC_GPIO_MODE_OUTPUT_ONLY);

    // Only configure SDA (GPIO3) for ULP reading
    rtc_gpio_init(SDA_GPIO);
    rtc_gpio_set_direction(SDA_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(SDA_GPIO); // Use hardware pullup
    rtc_gpio_pulldown_dis(SDA_GPIO);

    _initialized = true;
    Serial.println("  ULP: initialization complete");
}

void ULPManager::start()
{
    Serial.println("  ULP: starting program");

    // Always reload the ULP program when starting
    size_t size = sizeof(ulp_program) / sizeof(ulp_insn_t);
    esp_err_t err = ulp_process_macros_and_load(PROG_START, ulp_program, &size);
    if (err != ESP_OK)
    {
        Serial.printf("  ULP: program load error: %d\n", err);
        return;
    }

    // Stop timer first to ensure clean state
    // ulp_timer_stop();
    // Serial.println("  ULP: timer stopped");

    // // Set wakeup period - using period_index 0
    // err = ulp_set_wakeup_period(0, ULP_TIMER_PERIOD);
    // Serial.printf("  ULP: timer period set result: %d (ESP_OK=%d)\n", err, ESP_OK);
    // if (err != ESP_OK)
    // {
    //     Serial.printf("  ULP: timer period set error: %d\n", err);
    //     return;
    // }

    // // Resume timer before starting program
    // ulp_timer_resume();
    // Serial.println("  ULP: timer resumed");

    err = ulp_run(PROG_START);
    if (err != ESP_OK)
    {
        Serial.printf("  ULP: start error: %d\n", err);
        return;
    }
    Serial.println("  ULP: program started");
}

void ULPManager::stop()
{
    Serial.println("  ULP: stopping program");

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

    // Try to halt the ULP program
    ulp_timer_stop();
    Serial.println("  ULP: program stopped");
}

uint16_t ULPManager::getPIRCount()
{
    uint16_t count = (uint16_t)(RTC_SLOW_MEM[PIR_COUNT] & 0xFFFF);
    Serial.printf("  ULP: current PIR count: %d\n", count);
    return count;
}

void ULPManager::clearPIRCount()
{
    Serial.println("  ULP: clearing PIR count");
    RTC_SLOW_MEM[PIR_COUNT] = 0;
    Serial.printf("  ULP: verified count is now: %d\n", (uint16_t)(RTC_SLOW_MEM[PIR_COUNT] & 0xFFFF));
}