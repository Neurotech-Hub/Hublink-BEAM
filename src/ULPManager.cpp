#include "ULPManager.h"

// ULP program to count PIR trigger pulses (GPIO3)
const ulp_insn_t ulp_program[] = {
    I_MOVI(R2, PIR_COUNT), // R2 = address of counter
    I_LD(R1, R2, 0),       // R1 = current count

    M_LABEL(1),      // Main loop label
    I_DELAY(0xFFFF), // Max delay (~374.5µs)

    // Read GPIO3 state into R0
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If GPIO is not LOW, continue monitoring
    M_BG(1, 0), // if R0 > 0, continue monitoring

    // Motion detected (LOW), increment counter
    I_ADDI(R1, R1, 1), // Increment count
    I_ST(R1, R2, 0),   // Store updated count

    // Initialize delay counter
    I_MOVI(R3, 0), // R3 = delay counter

    // Delay loop for 1 second
    M_LABEL(2),        // Delay loop label
    I_DELAY(0xFFFF),   // Max delay (~374.5µs)
    I_ADDI(R3, R3, 1), // Increment delay counter
    I_MOVI(R0, 2669),  // Load comparison value
    M_BG(3, R0),       // If counter > 2669, exit delay
    M_BX(2),           // Otherwise continue delay loop

    M_LABEL(3), // Delay complete
    M_BX(1),    // Return to main monitoring loop
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

uint16_t ULPManager::getPIRCount()
{
    return (uint16_t)(RTC_SLOW_MEM[PIR_COUNT] & 0xFFFF);
}

void ULPManager::clearPIRCount()
{
    RTC_SLOW_MEM[PIR_COUNT] = 0;
}