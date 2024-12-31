#include "ZDP323.h"

volatile bool ZDP323::_motionDetected = false;

void IRAM_ATTR ZDP323::handleInterrupt()
{
    _motionDetected = true;
}

ZDP323::ZDP323(uint8_t i2cAddress)
    : _i2cAddress(i2cAddress), _initialized(false)
{
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    _config.fstep = ZDP323_CONFIG_FSTEP_2;
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B;
}

bool ZDP323::begin(TwoWire &wirePort)
{
    _wire = &wirePort;
    _wire->setTimeout(3000); // Set timeout to 3ms

    // Setup trigger pin with interrupt
    pinMode(ZDP323_TRIGGER_PIN, INPUT_PULLUP);

    // Step 1: Wait for power-on stabilization
    delay(500);

    Serial.println("Initializing PIR sensor...");

    // Step 2: Initial configuration with maximum threshold
    _config.detlvl = 0xFF; // Maximum threshold for initial setup
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    _config.fstep = ZDP323_CONFIG_FSTEP_2;        // Default filter step
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B; // Default filter type

    // Step 3 & 4: Write config and check Peak Hold until stable
    bool stable = false;
    const int maxAttempts = 10;
    int attempts = 0;

    while (!stable && attempts < maxAttempts)
    {
        Serial.printf("Configuration attempt %d of %d\n", attempts + 1, maxAttempts);

        if (!writeConfig())
        {
            Serial.println("Failed to write configuration");
            return false;
        }

        // Read Peak Hold value
        int16_t peakHold;
        if (!readPeakHold(&peakHold))
        {
            Serial.println("Failed to read peak hold value");
            return false;
        }

        // Check if below half threshold
        int16_t halfThreshold = (0xFF * 8) / 2;
        Serial.printf("Peak Hold: %d, Half Threshold: %d\n", peakHold, halfThreshold);

        if (abs(peakHold) < halfThreshold)
        {
            stable = true;
        }
        else
        {
            delay(ZDP323_TCYC_MS);
            attempts++;
        }
    }

    if (!stable)
    {
        Serial.println("Failed to achieve stable reading");
        return false;
    }

    // Step 5: Write final configuration with desired threshold and enable trigger mode
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
    _config.trigom = ZDP323_CONFIG_TRIGOM_ENABLED;
    Serial.println("Writing final configuration with trigger mode enabled...");
    if (!writeConfig())
    {
        Serial.println("Failed to write final configuration");
        return false;
    }

    // Step 6: Wait for sensor stability
    Serial.println("Waiting for sensor stability...");
    delay(ZDP323_TSTAB_MS);

    // Step 7: Setup interrupt for motion detection
    _initialized = true;
    _motionDetected = false;
    attachInterrupt(digitalPinToInterrupt(ZDP323_TRIGGER_PIN), handleInterrupt, FALLING);

    Serial.println("Successfully initialized PIR sensor");
    return true;
}

bool ZDP323::writeConfig()
{
    // Configuration is 56 bits (7 bytes) sent MSB first
    uint8_t data[7] = {0}; // All reserved bits default to 0

    // Byte 3: FILSEL (bits 26-28), FSTEP (bits 24-25), TRIGOM (bit 23)
    data[3] = ((_config.filsel & ZDP323_CONFIG_FILSEL_MASK) << 2) |
              (_config.fstep & ZDP323_CONFIG_FSTEP_MASK);

    // Byte 4: TRIGOM (bit 23) and upper bits of DETLVL (bits 22-16)
    data[4] = ((_config.trigom & ZDP323_CONFIG_TRIGOM_MASK) << 7) |
              ((_config.detlvl >> 1) & 0x7F);

    // Byte 5: Lower bit of DETLVL (bit 15) and reserved bits
    data[5] = (_config.detlvl & 0x01) << 7;

    Serial.println("Writing configuration data (56 bits):");
    Serial.printf("FILSEL = %d, FSTEP = %d, TRIGOM = %d, DETLVL = %d\n",
                  _config.filsel, _config.fstep, _config.trigom, _config.detlvl);

    for (int i = 0; i < 7; i++)
    {
        Serial.printf("Byte %d = 0x%02X\n", i, data[i]);
    }

    // Write configuration using General Call address
    _wire->beginTransmission(_i2cAddress);
    _wire->write(data, 7);
    uint8_t result = _wire->endTransmission(true);

    if (result != 0)
    {
        Serial.printf("Configuration write failed with error code: %d\n", result);
        return false;
    }

    Serial.println("Configuration write successful");
    return true;
}

bool ZDP323::readPeakHold(int16_t *peakHold)
{
    if (!peakHold)
    {
        Serial.println("Read peak hold failed: null pointer");
        return false;
    }

    // Request two bytes from the device
    uint8_t bytesRead = _wire->requestFrom(_i2cAddress, (uint8_t)2);
    if (bytesRead != 2)
    {
        Serial.printf("Failed to read peak hold data, requested 2 bytes but got %d\n", bytesRead);
        return false;
    }

    // Read the two bytes
    uint8_t msb = _wire->read(); // Upper byte (including 4 bits of peak hold)
    uint8_t lsb = _wire->read(); // Lower byte

    // Peak hold is a 12-bit signed value
    *peakHold = ((int16_t)(msb & 0x0F) << 8) | lsb;
    *peakHold <<= 4; // Sign extend
    *peakHold >>= 4;

    Serial.printf("Peak hold value: %d (MSB: 0x%02X, LSB: 0x%02X)\n", *peakHold, msb, lsb);
    return true;
}

bool ZDP323::isMotionDetected()
{
    if (!_initialized)
    {
        return false;
    }

    // Read and clear the motion flag
    bool motion = _motionDetected;
    _motionDetected = false;
    return motion;
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
            Serial.println("Filter step updated, waiting for stability...");
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
            Serial.println("Filter type updated, waiting for stability...");
            delay(ZDP323_TSTAB_MS);
        }
    }
}