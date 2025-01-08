#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp32s3/ulp.h"
#include "driver/rtc_io.h"
#include "soc/rtc_io_reg.h"

// Pin definitions from HublinkBEAM
#define SDA_GPIO GPIO_NUM_3

// ULP program constants
enum
{
    MOTION_FLAG, // RTC memory index for motion flag
    PROG_START   // Program start address
};

// ULP program to monitor PIR trigger (GPIO3) for LOW state with optimized delay
const ulp_insn_t ulp_program[] = {
    I_MOVI(R2, MOTION_FLAG), // set R2 to motion flag address

    M_LABEL(1),   // Main loop label
    I_DELAY(200), // ~11.4µs delay @ 17.5MHz (57ns per cycle)

    // Read GPIO3 state into R0
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If GPIO is LOW (motion detected), store state and halt
    M_BL(2, 1), // If R0 < 1 (LOW), branch to store/halt

    // Otherwise loop back to start
    M_BX(1), // Branch back to delay

    M_LABEL(2),      // Store and halt label
    I_MOVI(R0, 1),   // Load 1 into R0
    I_ST(R0, R2, 0), // Store 1 into motion flag
    I_HALT(),        // Halt until next timer wake
};

Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Motion flag value: " + String(RTC_SLOW_MEM[MOTION_FLAG]));

    // Initialize NeoPixel pins
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
    pixel.begin();
    pixel.clear();
    pixel.show();
    digitalWrite(NEOPIXEL_POWER, LOW);

    // Initialize and hold I2C power pin
    pinMode(PIN_I2C_POWER, OUTPUT);
    digitalWrite(PIN_I2C_POWER, HIGH);
    gpio_hold_en((gpio_num_t)PIN_I2C_POWER);

    // Clear RTC memory
    memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);

    // Configure SDA (GPIO3) for ULP reading
    rtc_gpio_init(SDA_GPIO);
    rtc_gpio_set_direction(SDA_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(SDA_GPIO); // Use hardware pullup
    rtc_gpio_pulldown_dis(SDA_GPIO);
    rtc_gpio_hold_en(SDA_GPIO);

    // Load and start ULP program
    size_t size = sizeof(ulp_program) / sizeof(ulp_insn_t);
    esp_err_t err = ulp_process_macros_and_load(PROG_START, ulp_program, &size);
    if (err != ESP_OK)
    {
        return;
    }

    err = ulp_run(PROG_START);
    if (err != ESP_OK)
    {
        return;
    }

    // Configure deep sleep wakeup timer (10 seconds)
    esp_sleep_enable_timer_wakeup(5000000); // microseconds
    esp_deep_sleep_start();
}

void loop()
{
    // Never reached - device restarts after deep sleep
}