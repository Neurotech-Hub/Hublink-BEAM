#include "ZDP323.h"
#define DEBUG 0 // Set to 0 to disable debug output

ZDP323::ZDP323(uint8_t i2cAddress)
    : _i2cAddress(i2cAddress), _initialized(false)
{
    // Initialize with default configuration
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;  // 0x40 (64 * 8 = ±512 ADC)
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED; // 0 (disabled)
    _config.fstep = ZDP323_CONFIG_FSTEP_2;          // 11 (Step 2)
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B;   // 000 (Type B)
}

bool ZDP323::begin(TwoWire &wirePort, bool isWakeFromSleep)
{
    Serial.println("  ZDP323: begin");
    _wire = &wirePort;
    _wire->setTimeout(3000);

    // If waking from sleep, we need to disable trigger mode first
    if (isWakeFromSleep)
    {
        Serial.println("  ZDP323: disabling trigger mode");
        _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
        if (!writeConfig())
        {
            Serial.println("  ZDP323: failed to disable trigger mode");
            return false;
        }
        _initialized = true;
        return true;
    }

    // Full initialization for first boot
    delay(500); // Only delay on first power-up

    // Initial configuration with maximum threshold and trigger mode disabled
    Serial.println("  ZDP323: initial config");
    _config.detlvl = 0xFF; // Maximum threshold during stabilization
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    _config.fstep = ZDP323_CONFIG_FSTEP_2;        // Step 2 (11)
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B; // Type B (000)

    // Write config and check Peak Hold until stable
    const int maxAttempts = 50;
    int attempts = 0;
    int configFailures = 0;
    const int maxConfigFailures = 10;

    while (attempts < maxAttempts)
    {
        if (!writeConfig())
        {
            configFailures++;
            if (configFailures >= maxConfigFailures)
            {
                Serial.printf("  ZDP323: config write failed (%d/%d)\n", configFailures, maxConfigFailures);
                return false;
            }
            delay(100);
            continue;
        }

        delay(ZDP323_TCYC_MS);

        int16_t peakHold;
        if (!readPeakHold(&peakHold))
        {
            attempts++;
            continue;
        }

        int16_t halfThreshold = (0xFF * 8) / 2;

        if (abs(peakHold) < halfThreshold)
        {
            Serial.println("  ZDP323: stability achieved");
            break; // Stability achieved
        }

        attempts++;
        delay(ZDP323_TCYC_MS * 10);
    }

    if (attempts >= maxAttempts)
    {
        Serial.println("  ZDP323: failed to achieve stability");
        return false;
    }

    // Write final configuration with desired threshold (trigger mode still disabled)
    Serial.println("  ZDP323: writing final config");
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
    if (!writeConfig())
    {
        Serial.println("  ZDP323: final config write failed");
        return false;
    }

    _initialized = true;
    Serial.println("  ZDP323: initialization complete");
    return true;
}

bool ZDP323::writeConfig()
{
    // Configuration is 56 bits (7 bytes) sent MSB first
    uint8_t data[7] = {0}; // All reserved bits default to 0

    // Byte 3: FILSEL (bits 26-28), FSTEP (bits 24-25)
    data[3] = ((_config.filsel & ZDP323_CONFIG_FILSEL_MASK) << 2) |
              (_config.fstep & ZDP323_CONFIG_FSTEP_MASK);

    // Byte 4: TRIGOM (bit 23) and upper bits of DETLVL (bits 22-16)
    data[4] = ((_config.trigom & ZDP323_CONFIG_TRIGOM_MASK) << 7) |
              (_config.detlvl >> 1);

    // Byte 5: Lower bit of DETLVL (bit 15) and reserved bits
    data[5] = (_config.detlvl & 0x01) << 7;

    Serial.printf("  I2C write: addr=0x%02X, data=[", _i2cAddress);
    for (int i = 0; i < 7; i++)
    {
        Serial.printf("0x%02X%s", data[i], i < 6 ? "," : "");
    }
    Serial.println("]");

    // Start transmission
    _wire->beginTransmission(_i2cAddress);
    _wire->write(data, 7);
    uint8_t result = _wire->endTransmission(true);

    if (result != 0)
    {
        Serial.printf("  I2C error: code=%d (%s)\n", result,
                      result == 1 ? "data too long" : result == 2 ? "NACK on addr"
                                                  : result == 3   ? "NACK on data"
                                                  : result == 4   ? "other error"
                                                                  : "unknown");
    }

    return result == 0;
}

bool ZDP323::readPeakHold(int16_t *peakHold)
{
    if (!peakHold)
    {
        Serial.println("  Peak hold: null pointer");
        return false;
    }

    Serial.printf("  I2C read: requesting 2 bytes from addr=0x%02X\n", _i2cAddress);

    // Request two bytes from the device
    uint8_t bytesRead = _wire->requestFrom(_i2cAddress, (uint8_t)2);
    if (bytesRead != 2)
    {
        Serial.printf("  I2C read error: requested 2 bytes, got %d\n", bytesRead);
        return false;
    }

    // Read the two bytes
    uint8_t msb = _wire->read();
    uint8_t lsb = _wire->read();
    Serial.printf("  I2C read: msb=0x%02X, lsb=0x%02X\n", msb, lsb);

    // Peak hold is a 12-bit signed value
    *peakHold = ((int16_t)(msb & 0x0F) << 8) | lsb;
    *peakHold <<= 4; // Sign extend
    *peakHold >>= 4;

    return true;
}

void ZDP323::setDetectionLevel(uint8_t level)
{
    _config.detlvl = level;
}

void ZDP323::setFilterStep(uint8_t step)
{
    if (_config.fstep != (step & ZDP323_CONFIG_FSTEP_MASK))
    {
        _config.fstep = step & ZDP323_CONFIG_FSTEP_MASK;
        if (writeConfig())
        {
            delay(ZDP323_TSTAB_MS);
        }
    }
}

void ZDP323::setFilterType(uint8_t type)
{
    if (_config.filsel != (type & ZDP323_CONFIG_FILSEL_MASK))
    {
        _config.filsel = type & ZDP323_CONFIG_FILSEL_MASK;
        if (writeConfig())
        {
            delay(ZDP323_TSTAB_MS);
        }
    }
}

bool ZDP323::enableTriggerMode()
{
    Serial.println("  ZDP323: enabling trigger mode");
    _config.trigom = ZDP323_CONFIG_TRIGOM_ENABLED;
    bool success = writeConfig();
    Serial.printf("  ZDP323: trigger mode enable %s\n", success ? "OK" : "FAILED");
    return success;
}

bool ZDP323::disableTriggerMode()
{
    Serial.println("  ZDP323: disabling trigger mode");
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    bool success = writeConfig();
    Serial.printf("  ZDP323: trigger mode disable %s\n", success ? "OK" : "FAILED");
    return success;
}
