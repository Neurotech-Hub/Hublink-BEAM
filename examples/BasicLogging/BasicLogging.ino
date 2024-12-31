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
  beam.logData();
  // beam.light_sleep(LOG_INTERVAL);
  delay(5000);
}