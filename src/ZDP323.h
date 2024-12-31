#ifndef ZDP323_H
#define ZDP323_H

#include <Arduino.h>
#include <Wire.h>

// Device addresses
#define ZDP323_I2C_ADDRESS_1 0x301 // ZDP323B1
#define ZDP323_I2C_ADDRESS_2 0x302 // ZDP323B2
#define ZDP323_I2C_ADDRESS_3 0x303 // ZDP323B3
#define ZDP323_I2C_ADDRESS_4 0x304 // ZDP323B4

// Default to first address (most common single-sensor configuration)
#define ZDP323_I2C_ADDRESS ZDP323_I2C_ADDRESS_1

// Configuration masks and defaults
#define ZDP323_CONFIG_DETLVL_MASK_MSB 0x7F
#define ZDP323_CONFIG_DETLVL_MASK_LSB 0x01
#define ZDP323_CONFIG_DETLVL_DEFAULT 0x1C
#define ZDP323_CONFIG_TRIGOM_DISABLED 0x00
#define ZDP323_CONFIG_TRIGOM_ENABLED 0x01
#define ZDP323_CONFIG_TRIGOM_MASK 0x01
#define ZDP323_CONFIG_FSTEP_1 0x01
#define ZDP323_CONFIG_FSTEP_2 0x03
#define ZDP323_CONFIG_FSTEP_3 0x00
#define ZDP323_CONFIG_FSTEP_MASK 0x03
#define ZDP323_CONFIG_FILSEL_TYPE_A 0x07
#define ZDP323_CONFIG_FILSEL_TYPE_B 0x00
#define ZDP323_CONFIG_FILSEL_TYPE_C 0x01
#define ZDP323_CONFIG_FILSEL_TYPE_D 0x02
#define ZDP323_CONFIG_FILSEL_DIRECT 0x03
#define ZDP323_CONFIG_FILSEL_MASK 0x07

// Counter registers
#define ZDP323_REG_COUNTER 0x08       // Counter register address
#define ZDP323_REG_COUNTER_RESET 0x09 // Counter reset register address

class ZDP323
{
public:
    // Constructor now accepts both enable pin and I2C address
    ZDP323(uint8_t enablePin = -1, uint16_t i2cAddress = ZDP323_I2C_ADDRESS);
    bool begin(TwoWire &wirePort = Wire);
    bool writeConfig();
    bool readPeakHold(int16_t *peakHold);
    void enable();
    void disable();

    // Configuration methods
    void setDetectionLevel(uint8_t level);
    void setTriggerOut(bool enabled);
    void setFilterStep(uint8_t step);
    void setFilterType(uint8_t type);

    // Counter methods
    uint32_t readAndResetCounter(); // Reads counter value and resets it

private:
    struct Config
    {
        uint8_t detlvl; // Detection Threshold Level
        uint8_t trigom; // Trigger Output Mode: 0-disabled, 1-enabled
        uint8_t fstep;  // Digital Band-Pass Filter Step Selection
        uint8_t filsel; // Digital Band-Pass Filter Type Selection
    } _config;

    TwoWire *_wire;
    uint8_t _enablePin;
    uint16_t _i2cAddress; // Store the I2C address
    bool _initialized;

    bool readCounter(uint32_t *count); // Raw counter read
    bool resetCounter();               // Reset counter to 0
};

#endif