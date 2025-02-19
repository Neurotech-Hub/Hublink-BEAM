#include "ULPManager.h"
#include "HublinkBEAM.h"

#define LED_PIN GPIO_NUM_13
#define LED_GPIO_INDEX 13

// ULP program to monitor PIR trigger (GPIO3) for LOW state with optimized delay
const ulp_insn_t ulp_program[] = {
    // Initialize registers
    I_MOVI(R2, PIR_COUNT), // R2 = PIR count address (preserve)
    I_MOVI(R3, 1),         // Initialize motion flag in R3 (1 = no motion detected yet)

    // Start of 1-second window
    M_LABEL(1),
    I_MOVI(R1, 40000), // Load sampling iteration count
    // I_WR_REG(RTC_GPIO_OUT_REG, LED_GPIO_INDEX + RTC_GPIO_OUT_DATA_S, LED_GPIO_INDEX + RTC_GPIO_OUT_DATA_S, 0), // LED off at start

    // Sampling loop
    M_LABEL(2),
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),
    M_BG(3, 0),    // If GPIO HIGH (no motion), continue to delay
    I_MOVI(R3, 0), // Set motion flag (0 = motion detected)

    // Delay and loop control
    M_LABEL(3),
    I_DELAY(415),      // ~25µs delay
    I_SUBI(R1, R1, 1), // Decrement iteration counter
    I_MOVR(R0, R1),    // Move counter to R0 for comparison
    M_BE(4, 0),        // If counter = 0, sampling window complete
    I_MOVR(R0, R3),    // Move motion flag to R0 for final processing
    M_BE(2, 1),        // If no motion detected yet (R3 = 1), continue sampling
    M_BX(3),           // Motion already detected, just continue delay

    // 1-second window complete - process results
    M_LABEL(4),
    I_MOVR(R0, R3), // Move motion flag to R0 for final processing
    M_BE(5, 0),     // If motion detected (R0 = 0), increment PIR count
    M_BX(6),        // Otherwise handle inactivity tracking

    // Increment PIR count
    M_LABEL(5),
    I_LD(R1, R2, 0),   // Load current PIR count
    I_ADDI(R1, R1, 1), // Increment count
    I_ST(R1, R2, 0),   // Store updated count
    M_BX(8),           // Reset tracker, start next window

    // Handle inactivity tracking
    M_LABEL(6),
    I_MOVI(R1, INACTIVITY_TRACKER), // Use R1 for tracker address
    I_LD(R0, R1, 0),                // Load current tracker value
    I_ADDI(R0, R0, 1),              // Increment tracker
    I_ST(R0, R1, 0),                // Store updated tracker
    I_MOVI(R1, INACTIVITY_PERIOD),  // Load period address
    I_LD(R1, R1, 0),                // Load period value into R1
    I_SUBR(R0, R1, R0),             // R0 =  period - tracker
    M_BG(9, 1),                     // If (period - tracker) > 1, start next window; else increment INACTIVITY_COUNT

    // Increment INACTIVITY_COUNT then reset tracker
    I_MOVI(R1, INACTIVITY_COUNT), // Load count address
    I_LD(R0, R1, 0),              // Load current INACTIVITY_COUNT
    I_ADDI(R0, R0, 1),            // Increment INACTIVITY_COUNT
    I_ST(R0, R1, 0),              // Store updated INACTIVITY_COUNT
    // I_WR_REG(RTC_GPIO_OUT_REG, LED_GPIO_INDEX + RTC_GPIO_OUT_DATA_S, LED_GPIO_INDEX + RTC_GPIO_OUT_DATA_S, 1), // LED off at start

    // Reset tracker
    M_LABEL(8),
    I_MOVI(R0, 0),                  // Set R0 to 0
    I_MOVI(R1, INACTIVITY_TRACKER), // Put INACTIVITY_TRACKER into R1
    I_ST(R0, R1, 0),                // Store/reset tracker

    M_LABEL(9),
    I_MOVI(R3, 1), // Reset motion flag for next window
    M_BX(1),       // Jump back to start of 1-second window
};

ULPManager::ULPManager()
{
    _initialized = false;
}

void ULPManager::begin()
{
    Serial.println("  ULP: begin");

    // Configure LED pin for debugging
    rtc_gpio_init((gpio_num_t)LED_BUILTIN);
    rtc_gpio_set_direction((gpio_num_t)LED_BUILTIN, RTC_GPIO_MODE_OUTPUT_ONLY);

    // Configure SD power pin
    rtc_gpio_init((gpio_num_t)PIN_SD_PWR_EN);
    rtc_gpio_set_direction((gpio_num_t)PIN_SD_PWR_EN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level((gpio_num_t)PIN_SD_PWR_EN, 0); // turn off SD power
    rtc_gpio_hold_en((gpio_num_t)PIN_SD_PWR_EN);

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
    // First disable holds
    rtc_gpio_hold_dis(SDA_GPIO);
    rtc_gpio_hold_dis((gpio_num_t)PIN_SD_PWR_EN);

    // Reset SDA pin configuration
    rtc_gpio_set_direction(SDA_GPIO, RTC_GPIO_MODE_DISABLED);
    rtc_gpio_pullup_dis(SDA_GPIO);
    rtc_gpio_pulldown_dis(SDA_GPIO);
    rtc_gpio_deinit(SDA_GPIO);

    rtc_gpio_set_direction((gpio_num_t)PIN_SD_PWR_EN, RTC_GPIO_MODE_DISABLED);
    rtc_gpio_deinit((gpio_num_t)PIN_SD_PWR_EN);

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