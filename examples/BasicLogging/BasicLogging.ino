#include <HublinkBEAM.h>

HublinkBEAM beam;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        delay(10);

    Serial.println("Hublink BEAM Basic Logging Example");

    // Initialize the device
    bool fullyInitialized = beam.begin();

    if (!fullyInitialized)
    {
        Serial.println("\nWARNING: Some sensors failed to initialize");
        Serial.println("The device will continue with limited functionality");
        Serial.println("Failed sensors will report error values in the log");
        delay(2000); // Give time to read the warning
    }
}

void loop()
{
    // Log data from all available sensors
    if (beam.logData())
    {
        Serial.println("Data logged successfully");
    }
    else
    {
        Serial.println("Failed to log data!");
    }

    // Basic delay between readings
    delay(1000);

    /* Sleep Mode Examples:
     *
     * 1. Light Sleep - CPU pauses but peripherals remain powered
     * Use this when you need quick wake-up and peripheral state preservation
     * Example: Sleep for 5 seconds
     * beam.light_sleep(5000);
     *
     * 2. Deep Sleep - Only RTC remains powered, system restarts on wake
     * Use this for maximum power saving when long sleep periods are acceptable
     * Example: Sleep for 1 minute
     * beam.deep_sleep(60000);
     *
     * Note: After deep_sleep, the device will restart and setup() will run again
     * You might want to check esp_reset_reason() to determine if it's a wake from
     * deep sleep vs a power-on reset
     */
}