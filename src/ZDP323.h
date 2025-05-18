#ifndef ZDP323_H
#define ZDP323_H

#include <Arduino.h>
#include <Wire.h>

// Device addresses
#define ZDP323_I2C_GENERAL_CALL 0x00 // General Call address for single-sensor setup
#define ZDP323_I2C_ADDRESS ZDP323_I2C_GENERAL_CALL

// Register addresses
#define ZDP323_REG_PEAK_HOLD 0x0A // Peak Hold register address

// Configuration defaults from documentation
#define ZDP323_CONFIG_DETLVL_DEFAULT 0x40  // B22-B15: 01000000 (threshold = ±512 ADC)
#define ZDP323_CONFIG_TRIGOM_DISABLED 0x00 // B23: 0 (disabled)
#define ZDP323_CONFIG_TRIGOM_ENABLED 0x01  // B23: 1 (enabled)
#define ZDP323_CONFIG_TRIGOM_MASK 0x01

// Filter step selection (B24-B25)
#define ZDP323_CONFIG_FSTEP_1 0x01 // Step 1 = 01
#define ZDP323_CONFIG_FSTEP_2 0x03 // Step 2 = 11 (default)
#define ZDP323_CONFIG_FSTEP_3 0x00 // Step 3 = 00
#define ZDP323_CONFIG_FSTEP_MASK 0x03

// Filter type selection (B26-B28)
#define ZDP323_CONFIG_FILSEL_TYPE_A 0x07 // Type A = 111
#define ZDP323_CONFIG_FILSEL_TYPE_B 0x00 // Type B = 000 (default)
#define ZDP323_CONFIG_FILSEL_TYPE_C 0x01 // Type C = 001
#define ZDP323_CONFIG_FILSEL_TYPE_D 0x02 // Type D = 010
#define ZDP323_CONFIG_FILSEL_DIRECT 0x03 // Direct = 011 (bypass)
#define ZDP323_CONFIG_FILSEL_MASK 0x07

// Timing constants
#define ZDP323_TSTAB_MS 10000 // Stability time (10 seconds)
#define ZDP323_TCYC_MS 10     // Minimum time between peak hold reads

class ZDP323
{
public:
    ZDP323(uint8_t i2cAddress = ZDP323_I2C_ADDRESS);
    bool begin(TwoWire &wirePort = Wire, bool isWakeFromSleep = false);
    bool writeConfig();
    bool enableTriggerMode();
    bool disableTriggerMode();

    // Configuration methods
    void setDetectionLevel(uint8_t level);
    void setFilterStep(uint8_t step);
    void setFilterType(uint8_t type);

private:
    bool readPeakHold(int16_t *peakHold);

    struct Config
    {
        uint8_t detlvl; // Detection Threshold Level
        uint8_t trigom; // Trigger Output Mode: 0-disabled, 1-enabled
        uint8_t fstep;  // Digital Band-Pass Filter Step Selection
        uint8_t filsel; // Digital Band-Pass Filter Type Selection
    } _config;

    TwoWire *_wire;
    uint8_t _i2cAddress;
    bool _initialized;
    unsigned long _lastPeakHoldRead;
};

#endif