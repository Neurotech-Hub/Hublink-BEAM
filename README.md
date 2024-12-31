# Hublink BEAM Arduino Library

An Arduino library for the Hublink BEAM ESP32-S3 data logging device. This library provides functionality for sensor monitoring and SD card data logging.

## Features

- Easy initialization of the Hublink BEAM device
- SD card data logging support
- Simple API for sensor monitoring (coming soon)

## Installation

1. Download this repository
2. In the Arduino IDE, go to Sketch -> Include Library -> Add .ZIP Library
3. Select the downloaded repository

## Usage

```cpp
#include <HublinkBEAM.h>

HublinkBEAM beam;

void setup() {
    beam.begin();
    beam.initSD();
}

void loop() {
    beam.logData("your,data,here");
    delay(1000);
}
```

## Examples

See the examples folder for detailed usage examples:
- BasicLogging: Demonstrates basic data logging functionality

## License

This library is released under the MIT License. See the LICENSE file for details.

## Author

Matt Gaidica (gaidica@wustl.edu) 