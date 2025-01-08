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
    I_LD(R3, R2, 0),         // Load current motion flag value into R3

    M_LABEL(1),   // New label for consistent halt point
    I_DELAY(438), // 25µs delay

    // Read GPIO3 state into R0 (we can reuse R0 now)
    I_RD_REG(RTC_GPIO_IN_REG, 3 + RTC_GPIO_IN_NEXT_S, 3 + RTC_GPIO_IN_NEXT_S),

    // If GPIO is not LOW (no motion), skip the motion handling
    M_BG(1, 0), // If R0 > 0 (HIGH), branch to label 1

    // Motion detected (GPIO is LOW)
    I_ADDI(R3, R3, 1), // Increment motion counter
    I_ST(R3, R2, 0),   // Store updated counter

    I_HALT(), // Always halt after one complete execution
};

Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void setup()
{
    ulp_timer_stop();
    Serial.begin(115200);
    delay(1000);
    int motionValue = RTC_SLOW_MEM[MOTION_FLAG];
    Serial.println("Motion flag value: " + String(motionValue));

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

    // Configure ULP to wake up every 1s
    ulp_set_wakeup_period(0, 1000000);

    err = ulp_run(PROG_START);
    if (err != ESP_OK)
    {
        return;
    }

    // Configure deep sleep wakeup timer (1 seconds)
    esp_sleep_enable_timer_wakeup(10000000); // microseconds
    esp_deep_sleep_start();
}

void loop()
{
    // Never reached - device restarts after deep sleep
}