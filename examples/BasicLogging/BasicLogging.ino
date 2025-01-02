#define DEBUG 1 // Set to 1 to enable debug delays

#include <HublinkBEAM.h>

HublinkBEAM beam;
const unsigned long LOG_INTERVAL = 10000; // Log every 5 seconds

void setup()
{
  Serial.begin(115200);
#if DEBUG
  delay(1000);
#endif

  if (!beam.begin())
  {
    while (1)
      delay(1000); // Halt if initialization failed
  }
}

void loop()
{
  /*
   * Motion logging requires calling sleep() to enable the ULP coprocessor monitoring.
   * The ULP program in ULPManager.cpp runs while the main processor is in deep sleep,
   * monitoring the PIR sensor's trigger pin (GPIO3/SDA). When motion is detected,
   * it sets a flag in RTC memory and halts until the next wake cycle.
   */
  beam.sleep(LOG_INTERVAL);
  // Note: Device will restart after deep sleep, returning to setup()

#if DEBUG
  delay(500);
#endif

  beam.logData();
}