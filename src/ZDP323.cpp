#include "ZDP323.h"
#define DEBUG 0 // Set to 0 to disable debug output

volatile bool ZDP323::_motionDetected = false;
volatile uint8_t ZDP323::_triggerPin = 0;

void IRAM_ATTR ZDP323::handleInterrupt()
{
    // Set motion flag and disable further interrupts
    _motionDetected = true;
    detachInterrupt(digitalPinToInterrupt(_triggerPin));

    // Visual indication (if LED is available)
    digitalWrite(LED_BUILTIN, HIGH);

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

#if DEBUG
    Serial.println("Initializing PIR sensor...");
#endif

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
#if DEBUG
        Serial.printf("Config attempt %d/%d\n", attempts + 1, maxAttempts);
#endif

        if (!writeConfig())
        {
            configFailures++;
#if DEBUG
            Serial.printf("Config write failed (%d/%d)\n", configFailures, maxConfigFailures);
#endif

            if (configFailures >= maxConfigFailures)
            {
#if DEBUG
                Serial.println("Max config failures exceeded");
#endif
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
#if DEBUG
            Serial.println("Peak hold read failed");
#endif
            attempts++;
            continue;
        }

        // Check if below half threshold
        int16_t halfThreshold = (0xFF * 8) / 2;
#if DEBUG
        Serial.printf("Peak hold: %d (threshold: ±%d)\n", peakHold, halfThreshold);
#endif

        if (abs(peakHold) < halfThreshold)
        {
#if DEBUG
            Serial.println("Stable reading achieved");
#endif
            break;
        }

        attempts++;
        delay(ZDP323_TCYC_MS);
    }

    if (attempts >= maxAttempts)
    {
#if DEBUG
        Serial.println("Failed to achieve stable reading");
#endif
        return false;
    }

    // Step 5: Write final configuration with desired threshold
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
#if DEBUG
    Serial.println("Writing final config...");
#endif

    if (!writeConfig())
    {
#if DEBUG
        Serial.println("Final config write failed");
#endif
        return false;
    }

#if DEBUG
    Serial.println("Waiting for stability...");
#endif
    delay(ZDP323_TSTAB_MS);

    _initialized = true;
    _motionDetected = false;

#if DEBUG
    Serial.println("PIR sensor init OK");
#endif
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

#if DEBUG
    Serial.printf("FILSEL = %d, FSTEP = %d, TRIGOM = %d, DETLVL = %d\n",
                  _config.filsel, _config.fstep, _config.trigom, _config.detlvl);
#endif

    // Start transmission
    _wire->beginTransmission(_i2cAddress);
    _wire->write(data, 7);
    uint8_t result = _wire->endTransmission(true);

    switch (result)
    {
    case 0:
#if DEBUG
        Serial.println("Configuration write successful");
#endif
        return true;
    case 1:
#if DEBUG
        Serial.println("I2C Error: Data too long");
#endif
        break;
    case 2:
#if DEBUG
        Serial.println("I2C Error: NACK on address");
#endif
        break;
    case 3:
#if DEBUG
        Serial.println("I2C Error: NACK on data");
#endif
        break;
    case 4:
#if DEBUG
        Serial.println("I2C Error: Other error");
#endif
        break;
    case 5:
#if DEBUG
        Serial.println("I2C Error: Timeout");
#endif
        break;
    default:
#if DEBUG
        Serial.printf("I2C Error: Unknown (%d)\n", result);
#endif
        break;
    }

    return false;
}

bool ZDP323::readPeakHold(int16_t *peakHold)
{
    if (!peakHold)
    {
#if DEBUG
        Serial.println("Read peak hold failed: null pointer");
#endif
        return false;
    }

    // Request two bytes from the device
    uint8_t bytesRead = _wire->requestFrom(_i2cAddress, (uint8_t)2);
    if (bytesRead != 2)
    {
#if DEBUG
        Serial.printf("Failed to read peak hold data, requested 2 bytes but got %d\n", bytesRead);
#endif
        return false;
    }

    // Read the two bytes
    uint8_t msb = _wire->read(); // Upper byte (including 4 bits of peak hold)
    uint8_t lsb = _wire->read(); // Lower byte

    // Peak hold is a 12-bit signed value
    *peakHold = ((int16_t)(msb & 0x0F) << 8) | lsb;
    *peakHold <<= 4; // Sign extend
    *peakHold >>= 4;

#if DEBUG
    Serial.printf("Peak hold value: %d (MSB: 0x%02X, LSB: 0x%02X)\n", *peakHold, msb, lsb);
#endif
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
#if DEBUG
            Serial.println("Filter step updated, waiting for stability...");
#endif
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
#if DEBUG
            Serial.println("Filter type updated, waiting for stability...");
#endif
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
#if DEBUG
        Serial.println("Attempting to enable trigger mode...");
#endif
        if (enableTriggerMode())
        {
            triggerEnabled = true;
#if DEBUG
            Serial.println("Trigger mode enabled successfully");
#endif
            delay(50);
            break;
        }
#if DEBUG
        Serial.println("Failed to enable trigger mode, retrying...");
#endif
        _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
        delay(100);
    }

    if (triggerEnabled)
    {
        attachInterrupt(digitalPinToInterrupt(_triggerPin), handleInterrupt, FALLING);
#if DEBUG
        Serial.println("Motion detection enabled");
#endif
    }
    else
    {
#if DEBUG
        Serial.println("Failed to enable motion detection - trigger mode could not be enabled");
#endif
        _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
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
