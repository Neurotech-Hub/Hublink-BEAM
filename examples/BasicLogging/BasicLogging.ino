#include <HublinkBEAM.h>

HublinkBEAM beam;
unsigned long lastMotionCheck = 0;
const unsigned long MOTION_CHECK_INTERVAL = 50; // Check every 50ms

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
    // Only check for motion at our defined interval
    unsigned long currentMillis = millis();
    if (currentMillis - lastMotionCheck >= MOTION_CHECK_INTERVAL)
    {
        lastMotionCheck = currentMillis;

        static bool lastMotionState = false;
        bool currentMotionState = beam.isMotionDetected();

        // Only print when state changes
        if (currentMotionState != lastMotionState)
        {
            if (currentMotionState)
            {
                Serial.println("Motion detected!");
                beam.setNeoPixel(NEOPIXEL_BLUE);
            }
            else
            {
                Serial.println("No motion");
                beam.disableNeoPixel();
            }
            lastMotionState = currentMotionState;
        }
    }
}