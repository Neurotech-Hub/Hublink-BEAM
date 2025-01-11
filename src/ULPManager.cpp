#include "ULPManager.h"

// ULP program to monitor PIR trigger (GPIO3) for LOW state with optimized delay
const ulp_insn_t ulp_program[] = {
    // Load addresses into registers we'll use throughout the program
    I_MOVI(R2, PIR_COUNT),          // R2 = PIR count address
    I_MOVI(R3, INACTIVITY_TRACKER), // R3 = Inactivity tracker address

    M_LABEL(1),
    I_DELAY(438), // debounce 25µs

    // Read GPIO3 state into R0
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If GPIO is not LOW (no motion), skip PIR increment and tracker reset
    M_BG(2, 0),        // If R0 > 0 (HIGH), branch to label 2
    I_LD(R0, R2, 0),   // Load current PIR count
    I_ADDI(R0, R0, 1), // Increment PIR count
    I_ST(R0, R2, 0),   // Store updated PIR count
    I_MOVI(R0, 0),     // Reset inactivity tracker
    I_ST(R0, R3, 0),   // Store reset tracker
    M_BX(3),           // Jump to delay section

    // Handle inactivity tracking
    M_LABEL(2),
    I_LD(R0, R3, 0),               // Load current tracker value
    I_ADDI(R0, R0, 1),             // Increment tracker
    I_ST(R0, R3, 0),               // Store updated tracker
    I_MOVI(R1, INACTIVITY_PERIOD), // Load period address
    I_LD(R1, R1, 0),               // Load period value
    M_BL(3, 1),                    // If tracker < period, skip to delay
    I_MOVI(R1, INACTIVITY_COUNT),  // Load count address
    I_LD(R0, R1, 0),               // Load current count
    I_ADDI(R0, R0, 1),             // Increment count
    I_ST(R0, R1, 0),               // Store updated count
    I_MOVI(R0, 0),                 // Reset tracker
    I_ST(R0, R3, 0),               // Store reset tracker

    // 1-second delay section
    M_LABEL(3),
    I_MOVI(R1, 270),   // Load iteration count into R1, 270 x 0xFFFF = 1s
    M_LABEL(4),        // Start of delay loop
    I_DELAY(0xFFFF),   // Maximum delay
    I_SUBI(R1, R1, 1), // Decrement counter in R1
    I_MOVR(R0, R1),    // Move R1 to R0 for comparison
    M_BG(4, 0),        // If R0 > 0 (R1 > 0), branch to delay loop
    M_BX(1),           // Jump back to main loop
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

    // Clear all counters
    clearPIRCount();
    clearInactivityCounters();

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

void ULPManager::setInactivityPeriod(uint16_t seconds)
{
    Serial.printf("  ULP: setting inactivity period to %d seconds\n", seconds);
    RTC_SLOW_MEM[INACTIVITY_PERIOD] = seconds;
}

uint16_t ULPManager::getInactivityCount()
{
    uint16_t count = (uint16_t)(RTC_SLOW_MEM[INACTIVITY_COUNT] & 0xFFFF);
    Serial.printf("  ULP: current inactivity count: %d\n", count);
    return count;
}

uint16_t ULPManager::getInactivityTracker()
{
    uint16_t tracker = (uint16_t)(RTC_SLOW_MEM[INACTIVITY_TRACKER] & 0xFFFF);
    Serial.printf("  ULP: current inactivity tracker: %d\n", tracker);
    return tracker;
}

void ULPManager::clearInactivityCounters()
{
    Serial.println("  ULP: clearing inactivity counters");
    RTC_SLOW_MEM[INACTIVITY_COUNT] = 0;
    RTC_SLOW_MEM[INACTIVITY_TRACKER] = 0;
    Serial.printf("  ULP: verified counters are now: count=%d, tracker=%d\n",
                  (uint16_t)(RTC_SLOW_MEM[INACTIVITY_COUNT] & 0xFFFF),
                  (uint16_t)(RTC_SLOW_MEM[INACTIVITY_TRACKER] & 0xFFFF));
}