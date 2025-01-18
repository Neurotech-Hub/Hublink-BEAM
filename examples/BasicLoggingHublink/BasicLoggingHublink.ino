#include <Hublink.h>
#include <HublinkBEAM.h>

HublinkBEAM beam;
Hublink hublink(PIN_SD_CS);
const unsigned long LOG_EVERY_MINUTES = 1;   // Log every X minutes
const unsigned long SYNC_EVERY_MINUTES = 10; // Sync every X minutes
const unsigned long SYNC_FOR_SECONDS = 30;   // Sync timeout in seconds

void setup()
{
  // Configure behavior
  beam.setNewFileOnBoot(false); // false to continue using same file if it's the same day
  beam.setInactivityPeriod(10); // 40 seconds; based on https://shorturl.at/JiZxK

  // Wait for the beam to initialize, retry (likely due to SD card ejecting)
  while (!beam.begin())
  {
    delay(1000); // Wait 1 second before retrying
  }
}

void loop()
{
  beam.logData();

  // Check if interval has passed (and set up alarm on first run)
  if (beam.alarm(SYNC_EVERY_MINUTES))
  {
    Serial.println("Alarm triggered!");

    // only begin when alarm is triggered (risks not catching error at setup though)
    if (hublink.begin())
    {
      Serial.println("✓ Hublink.");
      hublink.sync(SYNC_FOR_SECONDS);
    }
    else
    {
      Serial.println("✗ Failed.");
    }
  }

  /*
   * Motion logging requires calling sleep() to enable the ULP coprocessor monitoring.
   * The ULP program in ULPManager.cpp runs while the main processor is in deep sleep,
   * monitoring the PIR sensor's trigger pin (GPIO3/SDA). When motion is detected,
   * it sets a flag in RTC memory and halts until the next wake cycle. Note: Device
   * will restart after deep sleep, returning to setup(); nothing beyond beam.sleep()
   * will be executed.
   */
  beam.sleep(LOG_EVERY_MINUTES); // Sleep for LOG_EVERY_MINUTES minutes
}