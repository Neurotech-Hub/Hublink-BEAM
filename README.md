# Hublink BEAM Arduino Library

An Arduino library for the Hublink BEAM ESP32-S3 data logging device. This library provides functionality for sensor monitoring and SD card data logging.

## Installation

1. Download this repository
2. In the Arduino IDE, go to Sketch -> Include Library -> Add .ZIP Library
3. Select the downloaded repository

## Data Logging

### CSV Format
Each log entry contains the following columns:
- `datetime`: Current date and time (YYYY-MM-DD HH:MM:SS)
- `millis`: Milliseconds since last boot
- `battery_voltage`: Battery voltage in volts (-1.0 if sensor failed)
- `temperature_c`: Temperature in Celsius (-273.15 if sensor failed)
- `pressure_hpa`: Atmospheric pressure in hPa (-1.0 if sensor failed)
- `humidity_percent`: Relative humidity percentage (-1.0 if sensor failed)
- `lux`: Light level in lux (-1.0 if sensor failed)
- `pir_count`: Number of motion events since last log
- `reboot`: 1 if entry is from fresh boot, 0 if from wake from sleep

### File Creation Behavior
Files are named in the format `YYMMDDXX.csv` where:
- `YY`: Last two digits of year
- `MM`: Month (01-12)
- `DD`: Day (01-31)
- `XX`: Sequence number (00-99)

The sequence number handling can be controlled with the `newFileOnBoot` setting:
```cpp
beam.newFileOnBoot = false; // false to continue using same file if it's the same day
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
6. Creates a new log file with today's date and next available number (YYMMDDXX.csv)

## Deep Sleep Operation

The device uses a sophisticated two-stage deep sleep process to optimize power consumption while maintaining accurate motion detection:

### Stage 1: GPIO Monitoring
- Initial stage after `sleep()` is called
- Clears any existing PIR counts
- Stores sleep duration and start time in RTC memory
- Initializes (but doesn't start) ULP program
- Configures GPIO wakeup on SDA_GPIO (LOW)
- Enables PIR sensor's trigger mode
- Puts battery monitor into sleep mode
- Sets backup timer for full sleep duration
- Enters deep sleep waiting for GPIO interrupt

### Stage 2: ULP Monitoring
- Activated only if motion is detected during Stage 1
- Triggered by GPIO interrupt from PIR sensor
- Stops ULP to free GPIO pins for I2C
- Initializes I2C and RTC
- Increments motion counter for Stage 1 interrupt
- Calculates remaining sleep time
- Starts ULP program to count additional triggers
- Continues deep sleep for remaining duration

### Sleep State Management
- Sleep configuration stored in RTC memory
- Tracks original sleep duration and start time
- Preserves motion counts across sleep stages
- Handles both GPIO and timer wakeup sources
- Maintains I2C power state across stages
- Properly manages GPIO pin states between stages

### Benefits
- Minimal power usage during quiet periods (Stage 1)
- Accurate motion counting after initial trigger
- No missed events during stage transition
- Maintains original sleep duration timing
- Preserves sensor states across stages
- Clean GPIO and I2C management

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
- [x] Two-stage deep sleep for optimal power usage
- [ ] VEM7700 has low power modes (in library); reduces from ~13µA to ~5µA
- [x] BME280 has low power modes (in library); reduces from ~650µA to ~10µA
- [x] MAX17048 has hibernation mode (in library); reduces from ~23µA to ~4µA
- [x] SD card draws ~1200µA; could optimize
- [ ] SD card detent pull-up could be ~60-160µA when card is present
- [x] ZDP323 draws ~8µA (no low power mode)
- [ ] consider rtc_gpio_isolate(GPIO_NUM_12); from [esp-idf/examples/system/ulp/ulp_fsm/ulp/main/ulp_example_main.c at v5.4 · espressif/esp-idf](https://github.com/espressif/esp-idf/blob/v5.4/examples/system/ulp/ulp_fsm/ulp/main/ulp_example_main.c)

## License

This library is released under the MIT License. See the LICENSE file for details.

## Author

Matt Gaidica (gaidica@wustl.edu) 

---

The intent of the ULP is increment a counter when the GPIO goes low. Our PIR sensor sends a series of low pulses 50µs wide, often in bursts, when motion occurs. Therefore, we need to be able to detect the first of those pulses and increment the counter, then immediately I_HALT() to stop the ULP until the next timer wakeup. We are attempting to use a GPIO interrupt on the same line to wakeup from the first stage of deep sleep and turn on the ULP, in hopes of achieving very low power until the first motion event. This is why PIR_COUNT is set to 1 after that interrupt occurs, and why we only begin and start the ULP at that point in time. The ULP program:

1. Poll GPIO continuously until we see a LOW pulse
2. When LOW is detected, increment the counter once
3. Halt until the next timer wakeup
4. Repeat
