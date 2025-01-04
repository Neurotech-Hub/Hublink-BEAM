# Hublink BEAM Arduino Library

An Arduino library for the Hublink BEAM ESP32-S3 data logging device. This library provides functionality for sensor monitoring and SD card data logging.

## Installation

1. Download this repository
2. In the Arduino IDE, go to Sketch -> Include Library -> Add .ZIP Library
3. Select the downloaded repository

## Startup Sequence

On power-up or reset, the device:
1. Checks battery voltage (purple LED if battery < 3.4V)
2. Initializes SD card (must be present)
3. Sets up sensors and RTC
4. Creates a new log file with today's date and next available number (YYMMDDXX.csv)

LED Colors:
- Blue: Starting up
- Green: Motion detected (during data logging)
- Red: Error (SD card or sensor failure)
- Purple: Low battery
- Off: Normal operation

Debug Mode:
- Hold the BOOT button during operation to add delays for serial debugging

## Examples

See the examples folder for detailed usage examples:
- BasicLogging: Demonstrates basic data logging functionality

## Low Power Addons

- [x] Idle SD card by immediately checking for `/x.txt`
- [ ] Deep sleep without ULP until first interrupt?

### Sensors
- [ ] VEM7700 has low power modes (in library); reduces from ~13µA to ~5µA
- [x] BME280 has low power modes (in library); reduces from ~650µA to ~10µA
- [x] MAX17048 has hibernation mode (in library); reduces from ~23µA to ~4µA
- [x] SD card draws ~1200µA; could optimize
- [x] ZDP323 draws ~8µA (no low power mode)

## License

This library is released under the MIT License. See the LICENSE file for details.

## Author

Matt Gaidica (gaidica@wustl.edu) 