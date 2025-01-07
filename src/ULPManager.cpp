#include "ULPManager.h"

// ULP program to count PIR trigger pulses (GPIO3)
const ulp_insn_t ulp_program[] = {
    I_MOVI(R2, PIR_COUNT), // R2 = address of counter

    M_LABEL(1),      // Main loop label
    I_LD(R1, R2, 0), // R1 = current count from RTC memory

    M_LABEL(2),    // Polling loop
    I_DELAY(1000), // Small delay (~37µs) between reads

    // Read GPIO3 state into R0
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If HIGH, keep polling
    M_BG(2, 0), // If HIGH (R0 > 0), continue polling

    // Found LOW, increment counter
    I_ADDI(R1, R1, 1), // Increment count
    I_ST(R1, R2, 0),   // Store updated count

    I_HALT(), // Stop until next timer wakeup (1 second)
    M_BX(1),  // Return to main loop when timer wakes us
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

    // Only configure SDA (GPIO3) for ULP reading
    rtc_gpio_init(SDA_GPIO);
    rtc_gpio_set_direction(SDA_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(SDA_GPIO); // Use hardware pullup
    rtc_gpio_pulldown_dis(SDA_GPIO);
    rtc_gpio_hold_en(SDA_GPIO);

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

    // Configure ULP to wake up every 1 second (1,000,000 microseconds)
    ulp_set_wakeup_period(0, 1000000);

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
    ulp_set_wakeup_period(0, 0);

    delay(10); // Give some time for the pin to stabilize
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