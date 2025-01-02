# Hublink BEAM Arduino Library

An Arduino library for the Hublink BEAM ESP32-S3 data logging device. This library provides functionality for sensor monitoring and SD card data logging.

## Installation

1. Download this repository
2. In the Arduino IDE, go to Sketch -> Include Library -> Add .ZIP Library
3. Select the downloaded repository

## Examples

See the examples folder for detailed usage examples:
- BasicLogging: Demonstrates basic data logging functionality

## Low Power Addons

- [ ] VEM7700 has low power modes (in library); reduces from ~13µA to ~5µA
- [x] BME280 has low power modes (in library); reduces from ~650µA to ~10µA
- [ ] ZDP323 has low power modes?
- [ ] MAX17048 has hibernation mode (in library); reduces from ~23µA to ~4µA
- [ ] SD card draws ~1200µA; could optimize
- [x] ZDP323 draws ~8µA

## License

This library is released under the MIT License. See the LICENSE file for details.

## Author

Matt Gaidica (gaidica@wustl.edu) 