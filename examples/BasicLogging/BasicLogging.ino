#include <HublinkBEAM.h>

HublinkBEAM beam;
const unsigned long LOG_INTERVAL = 5000; // Log every 5 seconds

void setup()
{
  Serial.begin(115200);
  delay(1000);

  if (!beam.begin())
  {
    while (1)
      delay(1000); // Halt if initialization failed
  }
}

void loop()
{
  // Log data (this will automatically handle motion detection state)
  beam.logData();

  // Sleep until next logging interval
  // If motion is detected during sleep:
  // 1. Wake up to service the interrupt
  // 2. Disable further interrupts
  // 3. Sleep for remaining time
  // 4. Motion detection will be re-enabled in logData()
  beam.light_sleep(LOG_INTERVAL);
}