#include "ZDP323.h"

ZDP323::ZDP323(uint8_t enablePin, uint16_t i2cAddress)
    : _enablePin(enablePin), _i2cAddress(i2cAddress), _initialized(false)
{
    _config.detlvl = ZDP323_CONFIG_DETLVL_DEFAULT;
    _config.trigom = ZDP323_CONFIG_TRIGOM_DISABLED;
    _config.fstep = ZDP323_CONFIG_FSTEP_2;
    _config.filsel = ZDP323_CONFIG_FILSEL_TYPE_B;
}

bool ZDP323::begin(TwoWire &wirePort)
{
    _wire = &wirePort;

    if (_enablePin != -1)
    {
        pinMode(_enablePin, OUTPUT);
        enable();
        delay(1000); // Allow sensor to stabilize
    }

    // Initialize I2C
    _wire->begin();
    _wire->beginTransmission(_i2cAddress);
    if (_wire->endTransmission() != 0)
    {
        Serial.printf("Failed to communicate with PIR sensor at address 0x%03X\n", _i2cAddress);
        return false;
    }

    // Write default configuration
    if (!writeConfig())
    {
        Serial.println("Failed to write default configuration to PIR sensor");
        return false;
    }

    delay(2000); // Wait for configuration to take effect

    // Dummy read to clear the peak hold register
    int16_t dummy;
    if (!readPeakHold(&dummy))
    {
        Serial.println("Failed to perform initial peak hold read");
        return false;
    }

    _initialized = true;
    Serial.printf("Successfully initialized PIR sensor at address 0x%03X\n", _i2cAddress);
    return true;
}

bool ZDP323::writeConfig()
{
    uint8_t data[7] = {0};

    // Format configuration data according to sensor protocol
    data[3] = ((_config.filsel & ZDP323_CONFIG_FILSEL_MASK) << 2) |
              (_config.fstep & ZDP323_CONFIG_FSTEP_MASK);
    data[4] = ((_config.trigom & ZDP323_CONFIG_TRIGOM_MASK) << 7) |
              ((_config.detlvl >> 1) & ZDP323_CONFIG_DETLVL_MASK_MSB);
    data[5] = ((_config.detlvl & ZDP323_CONFIG_DETLVL_MASK_LSB) << 7);

    _wire->beginTransmission(_i2cAddress);
    _wire->write(data, 7);
    return (_wire->endTransmission() == 0);
}

bool ZDP323::readPeakHold(int16_t *peakHold)
{
    if (!peakHold)
        return false;

    if (_wire->requestFrom(_i2cAddress, (uint8_t)2) != 2)
    {
        return false;
    }

    uint8_t msb = _wire->read();
    uint8_t lsb = _wire->read();

    *peakHold = ((int16_t)(msb & 0x0F) << 8) | lsb;
    *peakHold <<= 4;
    *peakHold >>= 4;

    return true;
}

void ZDP323::enable()
{
    if (_enablePin != -1)
    {
        digitalWrite(_enablePin, HIGH);
    }
}

void ZDP323::disable()
{
    if (_enablePin != -1)
    {
        digitalWrite(_enablePin, LOW);
    }
}

void ZDP323::setDetectionLevel(uint8_t level)
{
    _config.detlvl = level;
}

void ZDP323::setTriggerOut(bool enabled)
{
    _config.trigom = enabled ? ZDP323_CONFIG_TRIGOM_ENABLED : ZDP323_CONFIG_TRIGOM_DISABLED;
}

void ZDP323::setFilterStep(uint8_t step)
{
    _config.fstep = step & ZDP323_CONFIG_FSTEP_MASK;
}

void ZDP323::setFilterType(uint8_t type)
{
    _config.filsel = type & ZDP323_CONFIG_FILSEL_MASK;
}

bool ZDP323::readCounter(uint32_t *count)
{
    if (!_initialized || !count)
    {
        return false;
    }

    _wire->beginTransmission(_i2cAddress);
    _wire->write(ZDP323_REG_COUNTER);
    if (_wire->endTransmission() != 0)
    {
        return false;
    }

    // Request 4 bytes (32-bit counter)
    if (_wire->requestFrom(_i2cAddress, (uint8_t)4) != 4)
    {
        return false;
    }

    // Read counter value (MSB first)
    *count = 0;
    for (int i = 0; i < 4; i++)
    {
        *count = (*count << 8) | _wire->read();
    }

    return true;
}

bool ZDP323::resetCounter()
{
    if (!_initialized)
    {
        return false;
    }

    _wire->beginTransmission(_i2cAddress);
    _wire->write(ZDP323_REG_COUNTER_RESET);
    _wire->write(0x00); // Any value will trigger the reset
    return (_wire->endTransmission() == 0);
}

uint32_t ZDP323::readAndResetCounter()
{
    uint32_t count = 0;

    // Try to read the counter
    if (!readCounter(&count))
    {
        count = 0; // Reset to 0 if read fails
    }

    // Always try to reset the counter, regardless of read success
    resetCounter();

    return count;
}