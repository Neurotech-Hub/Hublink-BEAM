#include "ZDP323.h"

volatile bool ZDP323::_motionDetected = false;

void IRAM_ATTR ZDP323::handleInterrupt()
{
    _motionDetected = true;
    detachInterrupt(digitalPinToInterrupt(ZDP323_TRIGGER_PIN));
    // Ensure trigger pulse completes (50µs + margin)
    delayMicroseconds(100);
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
    _wire->setTimeout(3000);

    // Step 1: Wait for power-on stabilization
    delay(500);

    Serial.println("Initializing PIR sensor...");

    // Step 2: Initial configuration with maximum threshold
    _config.detlvl = 0xFF;
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    _config.fstep = ZDP323_CONFIG_FSTEP_2;
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B;

    // Step 3 & 4: Write config and check Peak Hold until stable
    const int maxAttempts = 10;
    int attempts = 0;
    int configFailures = 0;
    const int maxConfigFailures = 3;

    while (attempts < maxAttempts)
    {
        Serial.printf("Config attempt %d/%d\n", attempts + 1, maxAttempts);

        if (!writeConfig())
        {
            configFailures++;
            Serial.printf("Config write failed (%d/%d)\n", configFailures, maxConfigFailures);

            if (configFailures >= maxConfigFailures)
            {
                Serial.println("Max config failures exceeded");
                return false;
            }
            delay(100);
            continue;
        }

        // Wait for a full cycle before reading
        delay(ZDP323_TCYC_MS);

        // Read Peak Hold value
        int16_t peakHold;
        if (!readPeakHold(&peakHold))
        {
            Serial.println("Peak hold read failed");
            attempts++;
            continue;
        }

        // Check if below half threshold
        int16_t halfThreshold = (0xFF * 8) / 2;
        Serial.printf("Peak hold: %d (threshold: ±%d)\n", peakHold, halfThreshold);

        if (abs(peakHold) < halfThreshold)
        {
            // Stable reading achieved, proceed to final config
            Serial.println("Stable reading achieved");
            break;
        }

        attempts++;
        delay(ZDP323_TCYC_MS);
    }

    if (attempts >= maxAttempts)
    {
        Serial.println("Failed to achieve stable reading");
        return false;
    }

    // Step 5: Write final configuration with desired threshold
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
    Serial.println("Writing final config...");

    if (!writeConfig())
    {
        Serial.println("Final config write failed");
        return false;
    }

    // Step 6: Wait for sensor stability
    Serial.println("Waiting for stability...");
    delay(ZDP323_TSTAB_MS);

    _initialized = true;
    _motionDetected = false;

    Serial.println("PIR sensor init OK");
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

    Serial.println("\nWriting configuration:");
    Serial.printf("FILSEL = %d, FSTEP = %d, TRIGOM = %d, DETLVL = %d\n",
                  _config.filsel, _config.fstep, _config.trigom, _config.detlvl);

    for (int i = 0; i < 7; i++)
    {
        Serial.printf("Byte %d = 0b", i);
        for (int bit = 7; bit >= 0; bit--)
        {
            Serial.print((data[i] >> bit) & 0x01);
        }
        Serial.println();
    }

    // Start transmission
    _wire->beginTransmission(_i2cAddress);
    _wire->write(data, 7);
    uint8_t result = _wire->endTransmission(true);

    switch (result)
    {
    case 0:
        Serial.println("Configuration write successful");
        return true;
    case 1:
        Serial.println("I2C Error: Data too long");
        break;
    case 2:
        Serial.println("I2C Error: NACK on address");
        break;
    case 3:
        Serial.println("I2C Error: NACK on data");
        break;
    case 4:
        Serial.println("I2C Error: Other error");
        break;
    case 5:
        Serial.println("I2C Error: Timeout");
        break;
    default:
        Serial.printf("I2C Error: Unknown (%d)\n", result);
        break;
    }

    return false;
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

    // Read and clear the flag
    bool motion = _motionDetected;
    _motionDetected = false;

    if (motion)
    {
        // Add a small delay before I2C communication
        delay(1);
        // Try to disable trigger mode with retries
        int retries = 3;
        while (retries-- > 0)
        {
            if (disableTriggerMode())
            {
                break;
            }
            delay(1);
        }
    }

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

void ZDP323::enableInterrupt(uint8_t pin)
{
    if (!_initialized)
        return;

    _triggerPin = pin;
    _motionDetected = false;

    // First, ensure interrupts are detached
    detachInterrupt(digitalPinToInterrupt(_triggerPin));

    // Add longer delay after initialization to ensure complete stability
    delay(100);

    // Try to enable trigger mode with retries
    int retries = 3;
    bool triggerEnabled = false;

    while (retries-- > 0)
    {
        Serial.println("Attempting to enable trigger mode...");
        if (enableTriggerMode())
        {
            triggerEnabled = true;
            Serial.println("Trigger mode enabled successfully");
            // Add stability delay after successful trigger mode enable
            delay(50);
            break;
        }
        Serial.println("Failed to enable trigger mode, retrying...");
        // Reset trigger mode state since write failed
        _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
        delay(100); // Longer delay between retries
    }

    if (triggerEnabled)
    {
        // Now safe to attach interrupt
        attachInterrupt(digitalPinToInterrupt(_triggerPin), handleInterrupt, FALLING);
        Serial.println("Motion detection enabled");
    }
    else
    {
        Serial.println("Failed to enable motion detection - trigger mode could not be enabled");
        _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED; // Ensure config reflects actual state
    }
}

void ZDP323::disableInterrupt(uint8_t pin)
{
    if (_initialized && _triggerPin == pin)
    {
        detachInterrupt(digitalPinToInterrupt(_triggerPin));
        disableTriggerMode();
    }
}

bool ZDP323::disableTriggerMode()
{
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    return writeConfig();
}

bool ZDP323::enableTriggerMode()
{
    _config.trigom = ZDP323_CONFIG_TRIGOM_ENABLED;
    return writeConfig();
}
