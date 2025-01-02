#include "ZDP323.h"
#define DEBUG 0 // Set to 0 to disable debug output

ZDP323::ZDP323(uint8_t i2cAddress)
    : _i2cAddress(i2cAddress), _initialized(false)
{
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    _config.fstep = ZDP323_CONFIG_FSTEP_2;
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B;
}

bool ZDP323::begin(TwoWire &wirePort, bool isWakeFromSleep)
{
    Serial.println("      - Setting up I2C connection...");
    _wire = &wirePort;
    _wire->setTimeout(3000);

    // If waking from sleep, we need to disable trigger mode first
    if (isWakeFromSleep)
    {
        Serial.println("      - Wake from sleep, disabling trigger mode");
        _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
        if (!writeConfig())
        {
            Serial.println("      - Failed to disable trigger mode");
            return false;
        }
        _initialized = true;
        return true;
    }

    // Full initialization for first boot
    Serial.println("      - Starting full initialization");
    delay(500); // Only delay on first power-up

    Serial.println("      - Setting initial config");
    // Initial configuration with maximum threshold and trigger mode disabled
    _config.detlvl = 0xFF;
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    _config.fstep = ZDP323_CONFIG_FSTEP_2;
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B;

    // Write config and check Peak Hold until stable
    const int maxAttempts = 10;
    int attempts = 0;
    int configFailures = 0;
    const int maxConfigFailures = 10;

    Serial.println("      - Starting stability check loop");
    while (attempts < maxAttempts)
    {
        Serial.printf("        Attempt %d/%d\n", attempts + 1, maxAttempts);

        if (!writeConfig())
        {
            configFailures++;
            Serial.printf("        Config write failed (%d/%d)\n", configFailures, maxConfigFailures);
            if (configFailures >= maxConfigFailures)
            {
                Serial.println("        Max config failures exceeded");
                return false;
            }
            delay(100);
            continue;
        }

        delay(ZDP323_TCYC_MS);

        int16_t peakHold;
        if (!readPeakHold(&peakHold))
        {
            Serial.println("        Peak hold read failed");
            attempts++;
            continue;
        }

        int16_t halfThreshold = (0xFF * 8) / 2;
        Serial.printf("        Peak hold: %d (threshold: ±%d)\n", peakHold, halfThreshold);

        if (abs(peakHold) < halfThreshold)
        {
            Serial.println("        Stability achieved");
            break;
        }

        attempts++;
        delay(ZDP323_TCYC_MS);
    }

    if (attempts >= maxAttempts)
    {
        Serial.println("        Failed to achieve stability");
        return false;
    }

    Serial.println("      - Writing final configuration");
    // Write final configuration with desired threshold (trigger mode still disabled)
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
    if (!writeConfig())
    {
        Serial.println("        Final config write failed");
        return false;
    }

    _initialized = true;
    Serial.println("      - PIR initialization complete");
    return true;
}

bool ZDP323::writeConfig()
{
    Serial.println("        Writing config...");
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

    Serial.printf("        FILSEL=%d, FSTEP=%d, TRIGOM=%d, DETLVL=%d\n",
                  _config.filsel, _config.fstep, _config.trigom, _config.detlvl);

    // Start transmission
    _wire->beginTransmission(_i2cAddress);
    _wire->write(data, 7);
    uint8_t result = _wire->endTransmission(true);

    if (result == 0)
    {
        Serial.println("        Config write successful");
    }
    else
    {
        Serial.printf("        Config write failed with error %d\n", result);
    }

    return result == 0;
}

bool ZDP323::readPeakHold(int16_t *peakHold)
{
    if (!peakHold)
    {
        Serial.println("        Peak hold read failed: null pointer");
        return false;
    }

    // Request two bytes from the device
    uint8_t bytesRead = _wire->requestFrom(_i2cAddress, (uint8_t)2);
    if (bytesRead != 2)
    {
        Serial.printf("        Peak hold read failed: requested 2 bytes, got %d\n", bytesRead);
        return false;
    }

    // Read the two bytes
    uint8_t msb = _wire->read(); // Upper byte (including 4 bits of peak hold)
    uint8_t lsb = _wire->read(); // Lower byte

    // Peak hold is a 12-bit signed value
    *peakHold = ((int16_t)(msb & 0x0F) << 8) | lsb;
    *peakHold <<= 4; // Sign extend
    *peakHold >>= 4;

    Serial.printf("        Peak hold read: %d (MSB: 0x%02X, LSB: 0x%02X)\n", *peakHold, msb, lsb);
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
    _config.trigom = ZDP323_CONFIG_TRIGOM_ENABLED;
    return writeConfig();
}

bool ZDP323::disableTriggerMode()
{
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    return writeConfig();
}
