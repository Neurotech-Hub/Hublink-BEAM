#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp32s3/ulp.h"
#include "driver/rtc_io.h"
#include "soc/rtc_io_reg.h"

// Pin definitions from HublinkBEAM
#define SDA_GPIO GPIO_NUM_3
#define PIN_SD_PWR_EN 10 // SD card VDD LDO enable

// ULP program constants
enum
{
    MOTION_FLAG, // RTC memory index for motion flag
    PROG_START   // Program start address
};

// ULP program to monitor PIR trigger (GPIO3) for LOW state with optimized delay
const ulp_insn_t ulp_program[] = {
    I_HALT(), // Halt until next timer wake
};

Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

void setup()
{
    // Initialize NeoPixel pins
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
    pixel.begin();
    pixel.clear();
    pixel.show();
    digitalWrite(NEOPIXEL_POWER, LOW);

    // Initialize and hold I2C power pin
    pinMode(PIN_SD_PWR_EN, OUTPUT);
    digitalWrite(PIN_SD_PWR_EN, LOW);
    gpio_hold_en((gpio_num_t)PIN_SD_PWR_EN);

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
    esp_sleep_enable_timer_wakeup(10000000); // microseconds
    esp_deep_sleep_start();
}

void loop()
{
    // Never reached - device restarts after deep sleep
}