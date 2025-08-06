# Hublink BEAM Arduino Library

An Arduino library for the Hublink BEAM ESP32-S3 data logging device. This library provides functionality for sensor monitoring and SD card data logging.

## Installation

1. In Arduino IDE, you use Boards Manager to install the Espressif `esp32` board package.

2. Install the Hublink-BEAM library from Arduino IDE. Alternatively (but not recommended), clone or download this repository to the libraries folder in the Arduino IDE, (or use Sketch -> Include Library -> Add .ZIP Library).

3. Ensure the dependencies are installed for [library.properties](library.properties) and [https://github.com/Neurotech-Hub/Hublink-Node](https://github.com/Neurotech-Hub/Hublink-Node):
- RTClib
- Adafruit MAX1704X
- Adafruit BME280 Library
- Adafruit VEML7700 Library
- Adafruit NeoPixel
- ESP32Time
- Preferences

**meta.json File Contents**
BEAM must have a `meta.json` file present to control timing parameters (if deviating from the sketch defaults) and sync with Hublink. Several Hublink parameters are disregarded because of BEAM's deep sleep cycling: `advertise_every`, `advertise_for`, and reconnect settings are not utilized but can still be placed in the hublink structure.

```
json
{
  "hublink": {
    "advertise": "HUBLINK",
    "advertise_every": 120,
    "advertise_for": 30,
    "try_reconnect": false,
    "reconnect_attempts": 2,
    "reconnect_every": 30,
    "upload_path": "/BEAM",
    "append_path": "device:id",
    "disable": false
  },
  "beam": {
    "log_every_minutes": 1,
    "sync_every_minutes": 3,
    "sync_for_seconds": 30,
    "new_file_on_boot": true,
    "inactivity_period_seconds": 40
  },
  "subject": {
    "id": "",
    "strain": "",
    "sex": "F"
  },
  "experiment": {
    "name": ""
  },
  "device": {
    "id": "001"
  }
}
```

**Testing**
1. Ensure the power switch is in the `ON` position.

2. If the device has been previously flashed, it may be in a deep sleep state and will not connect to the serial port. To enter boot mode, hold the `Boot` button and toggle the `Reset` button (then release the `Boot` button).

3. If the Arduino IDE does not indicate that it is connected to "Adafruit Feather ESP32-S3 2MB PSRAM", click Tools -> Board -> esp32 -> Adafruit Feather ESP32-S3 2MB PSRAM (you will need to download the esp32 board package (by espressif) from the Arduino IDE).

### Debug Mode
You may enable debugging mode by placing the `A` switch down. This will reduce the PIR sensor initialization period and introduce delays before Serial statements so they can be read by a serial terminal. These delays are useful for debugging, but should be removed for normal operation.

### RTC Setting
Setting the RTC is performed using the compilation time of the sketch. This is not always accurate and typically requires a clearing of your sketch cache to ensure correctness.

**Clear Arduino IDE Cache:**

On **MacOS**:
```bash
rm -rf /Users/<username>/Library/Caches/arduino/sketches
```

On **Windows**:
```cmd
rmdir /s "%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\*\cache"
del /s /q "%TEMP%\arduino_*"
```

On **Linux**:
```bash
rm -rf ~/.arduino15/packages/esp32/tools/esptool_py/*/cache
rm -rf /tmp/arduino_*
```

**Alternative RTC Setting Methods:**

1. **Manual RTC Setting** (more accurate):
   ```cpp
   // Set RTC to specific date/time
   beam.adjustRTC(DateTime(2024, 1, 15, 10, 30, 0)); // Year, Month, Day, Hour, Minute, Second
   ```

2. **Unix Timestamp Setting**:
   ```cpp
   // Set RTC using Unix timestamp
   beam.adjustRTC(1705315800); // Unix timestamp
   ```

3. **Ensure Fresh Compilation**:
   - Close Arduino IDE completely
   - Clear cache as shown above
   - Reopen IDE and compile/upload immediately for most accurate compilation time

## Hardware Assembly
For detailed hardware assembly instructions, including the required low-power modification, see [docs/assembly.md](docs/assembly.md).

### LED Indicators
These indicators may be temporary during operations or flash continuously if stuck in a loop.

- Blue: Starting up
- Red: Error (SD card or sensor failure)
- Green: Motion detected (during data logging)
- Purple: Low battery
- Off: Normal operation

## Data Logging

### CSV Format
Each log entry contains the following columns:
- `datetime`: Current date and time (YYYY-MM-DD HH:MM:SS)
- `millis`: Milliseconds since last boot
- `device_id`: Device identifier (3-character alphanumeric)
- `library_version`: Library version string
- `battery_voltage`: Battery voltage in volts (-1.0 if sensor failed)
- `temperature_c`: Temperature in Celsius (-273.15 if sensor failed)
- `pressure_hpa`: Atmospheric pressure in hPa (-1.0 if sensor failed)
- `humidity_percent`: Relative humidity percentage (-1.0 if sensor failed)
- `lux`: Light level in lux (-1.0 if sensor failed)
- `activity_count`: Number of motion events since last log
- `activity_percent`: Fraction of time PIR was active (0-1)
- `inactivity_period_s`: Configured inactivity period in seconds
- `inactivity_count`: Number of complete inactivity periods
- `inactivity_percent`: Fraction of possible inactivity periods (0-1)
- `min_free_heap`: Minimum free heap memory in bytes
- `reboot`: 1 if entry is from fresh boot, 0 if from wake from sleep

### File Creation Behavior
Files are named in the format `/BEAM_YYYYMMDDXX.csv` where:
- `YYYY`: Year
- `MM`: Month (01-12)
- `DD`: Day (01-31)
- `XX`: Sequence number (00-99)

For a detailed flowchart of the filename selection logic, see [docs/filename_logic.md](docs/filename_logic.md).

The sequence number handling can be controlled with the `setNewFileOnBoot()` method:
```cpp
beam.setNewFileOnBoot(false); // false to continue using same file if it's the same day
```

- When `true` (default): Creates a new file with incremented sequence number on each boot
- When `false`: Continues using the same file if it's from the same day

If the SD card is cleared or files are deleted:
1. The system scans for existing files matching today's date
2. Uses the next available sequence number (00-99)
3. Creates a new file regardless of any stored filename in preferences
4. This ensures proper sequencing even after data is wiped

## Startup Sequence

On power-up or reset, the device:
1. Stops any running ULP program to free GPIO pins
2. Initializes pins and I2C power
3. Checks battery voltage (purple LED if battery < 3.4V)
4. Initializes SD card (must be present)
5. Sets up sensors and RTC
6. Creates a new log file with today's date and next available number

## Deep Sleep Operation

The device uses the ESP32's ULP (Ultra Low Power) coprocessor to monitor motion while the main processor is in deep sleep. See [docs/flowchart.md](docs/flowchart.md) for a detailed flow diagram.

### Inactivity Period Configuration
The inactivity period can be set using the `setInactivityPeriod()` method:
```cpp
beam.setInactivityPeriod(40); // 40 seconds of immobility indicates sleep
```

The 40-second threshold is based on research by [Brown et al. (2017)](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5140024/) which demonstrated that extended immobility of >40 seconds provides a reliable indicator of sleep, correlating well with EEG-defined sleep (Pearson's r >0.95, n=4 mice).

### ULP Operation
- ULP program continuously monitors the PIR sensor
- Increments PIR count when motion is detected
- Tracks inactivity periods based on configured duration
- Maintains counters in RTC memory
- Main processor reads counters upon waking

### Sleep Process
- Configures ULP program with current settings
- Disables sensors and peripherals to save power
- Enters deep sleep for specified duration
- Wakes on timer expiration to log data
- Calculates activity metrics upon wake

## Examples

See the examples folder for detailed usage examples:
- BasicLogging: Demonstrates basic data logging functionality

## Low Power Addons

- [x] Idle SD card by immediately checking for `/x.txt`
- [x] Two-stage deep sleep for optimal power usage
- [ ] VEM7700 has low power modes (in library); reduces from ~13µA to ~5µA
- [x] BME280 has low power modes (in library); reduces from ~650µA to ~10µA
- [x] MAX17048 has hibernation mode (in library); reduces from ~23µA to ~4µA
- [x] SD card draws ~1200µA; could optimize
- [ ] SD card detent pull-up could be ~60-160µA when card is present; consider toggling pull-up to check
- [x] ZDP323 draws ~8µA (no low power mode)
- [ ] consider rtc_gpio_isolate(GPIO_NUM_12); from [esp-idf/examples/system/ulp/ulp_fsm/ulp/main/ulp_example_main.c at v5.4 · espressif/esp-idf](https://github.com/espressif/esp-idf/blob/v5.4/examples/system/ulp/ulp_fsm/ulp/main/ulp_example_main.c)

## License and Copyright

Copyright © 2024 Neurotech Hub at Washington University in St. Louis. All rights reserved.

This software is proprietary and confidential. The source code is the property of the Neurotech Hub at Washington University in St. Louis and is protected by intellectual property laws. No part of this software may be deployed, copied, modified, or distributed in any form or by any means without the prior written permission from the Neurotech Hub at Washington University in St. Louis.

For licensing inquiries, please contact the Neurotech Hub at Washington University in St. Louis.
