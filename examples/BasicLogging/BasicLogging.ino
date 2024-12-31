#include <HublinkBEAM.h>

HublinkBEAM beam;
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 60000; // Log every minute

void setup()
{
    Serial.begin(115200);
    delay(2000); // Allow time for serial monitor to open

    Serial.println("Initializing BEAM system...");
    if (!beam.begin())
    {
        Serial.println("Failed to initialize BEAM system!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("BEAM system initialized successfully!");
}

void loop()
{
    // Check for motion (interrupt automatically disabled when detected)
    if (beam.isMotionDetected())
    {
        Serial.println("Motion detected!");
        beam.setNeoPixel(NEOPIXEL_BLUE);

        delay(100); // Small delay to avoid rapid toggling
        beam.disableNeoPixel();

        // Re-enable motion detection after handling the event
        beam.enableMotionDetection();
    }

    // Regular interval logging
    // unsigned long currentMillis = millis();
    // if (currentMillis - lastLogTime >= LOG_INTERVAL)
    // {
    //     lastLogTime = currentMillis;

    //     // Temporarily disable motion detection during logging
    //     beam.disableMotionDetection();

    //     if (beam.logData())
    //     {
    //         Serial.println("Regular log completed successfully");
    //     }
    //     else
    //     {
    //         Serial.println("Failed to complete regular log");
    //     }

    //     // Re-enable motion detection after logging
    //     beam.enableMotionDetection();
    // }
}