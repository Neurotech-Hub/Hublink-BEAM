#include <Hublink.h>
#include <HublinkBEAM.h>

HublinkBEAM beam;
Hublink hublink(PIN_SD_CS);

// default values
int LOG_EVERY_MINUTES = 10;         // Log every X minutes
int SYNC_EVERY_MINUTES = 30;        // Sync every X minutes
int SYNC_FOR_SECONDS = 30;          // Sync timeout in seconds
bool NEW_FILE_ON_BOOT = true;       // Create new file on boot
int INACTIVITY_PERIOD_SECONDS = 40; // Inactivity period in seconds

// Hublink callback function to handle timestamp
void onTimestampReceived(uint32_t timestamp)
{
  Serial.println("Received timestamp: " + String(timestamp));
  beam.adjustRTC(timestamp);
}

void setup()
{
  // only begin when alarm is triggered (risks not catching error at setup though)
  if (hublink.begin())
  {
    Serial.println("✓ Hublink.");
    hublink.setTimestampCallback(onTimestampReceived);

    // override default values with values from meta.json
    if (hublink.hasMetaKey("beam", "log_every_minutes"))
    {
      LOG_EVERY_MINUTES = hublink.getMeta<int>("beam", "log_every_minutes");
    }
    if (hublink.hasMetaKey("beam", "sync_every_minutes"))
    {
      SYNC_EVERY_MINUTES = hublink.getMeta<int>("beam", "sync_every_minutes");
    }
    if (hublink.hasMetaKey("beam", "sync_for_seconds"))
    {
      SYNC_FOR_SECONDS = hublink.getMeta<int>("beam", "sync_for_seconds");
    }
    if (hublink.hasMetaKey("beam", "new_file_on_boot"))
    {
      NEW_FILE_ON_BOOT = hublink.getMeta<bool>("beam", "new_file_on_boot");
    }
    if (hublink.hasMetaKey("beam", "inactivity_period_seconds"))
    {
      INACTIVITY_PERIOD_SECONDS = hublink.getMeta<int>("beam", "inactivity_period_seconds");
    }
  }
  else
  {
    Serial.println("✗ Hublink Failed.");
  }
  // Configure behavior
  beam.setNewFileOnBoot(NEW_FILE_ON_BOOT);             // false to continue using same file if it's the same day
  beam.setInactivityPeriod(INACTIVITY_PERIOD_SECONDS); // 40 seconds; based on https://shorturl.at/JiZxK

  // Wait for the beam to initialize, retry (likely due to SD card ejecting)
  while (!beam.begin())
  {
    Serial.println("✗ BEAM Failed.");
    delay(1000); // Wait 1 second before retrying
  }

  // we need BEAM begin to access switchBDown() and setNeoPixel()
  // but this will write a new row/file
  bool didSync = false;
  while (beam.switchBDown())
  {
    beam.setNeoPixel(NEOPIXEL_OFF);
    if (!didSync)
    {
      didSync = hublink.sync(SYNC_FOR_SECONDS);
      delay(100); // repeat until sync is successful
    }
    else
    {
      // blink white after sync is successful
      beam.setNeoPixel(NEOPIXEL_WHITE);
      delay(100);
      beam.setNeoPixel(NEOPIXEL_OFF);
      delay(1000);
    }
  }

  beam.setLightGain(VEML7700_GAIN_2);
  beam.setLightIntegrationTime(VEML7700_IT_800MS);
}

void loop()
{
  beam.logData();

  // Check if interval has passed (and set up alarm on first run)
  if (beam.alarm(SYNC_EVERY_MINUTES))
  {
    Serial.println("Alarm triggered!");
    // this is a forced sync, so it will ignore some of the settings in meta.json
    // such as: advertise_every, advertise_for, or try_reconnect
    hublink.sync(SYNC_FOR_SECONDS);
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