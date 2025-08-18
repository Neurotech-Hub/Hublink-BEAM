#include <Hublink.h>
#include <HublinkBEAM.h>

HublinkBEAM beam;
Hublink hublink(PIN_SD_CS);

// default values, !! overriden by meta.json !!
int LOG_EVERY_MINUTES = 10;         // Log every X minutes
int SYNC_EVERY_MINUTES = 30;        // Sync every X minutes
int SYNC_FOR_SECONDS = 30;          // Sync timeout in seconds
bool NEW_FILE_ON_BOOT = true;       // Create new file on boot
int INACTIVITY_PERIOD_SECONDS = 40; // Inactivity period in seconds
int RANDOMIZE_ALARM_MINUTES = 0;    // Alarm randomization in minutes (0 = disabled)
String DEVICE_ID = "XXX";           // Default device ID (3 characters)

// Hublink callback function to handle timestamp
void onTimestampReceived(uint32_t timestamp)
{
  Serial.println("Received timestamp: " + String(timestamp));
  beam.adjustRTC(timestamp);
}

void setup()
{
  // Init beam (and de-init ULP coprocessor), retry on failure (likely due to empty SD card)
  while (!beam.begin())
  {
    Serial.println("✗ BEAM Failed.");
    beam.setNeoPixel(NEOPIXEL_RED);
    delay(1000); // Wait 1 second before retrying
  }
  beam.setNeoPixel(NEOPIXEL_OFF);

  // we don't check for hublink success here because we want to continue setup even if hublink fails
  beginHublink();          // reads meta.json and overrides default values if found
  syncUnlessSwitchBDown(); // force hublink sync if switch B is down

  beam.setInactivityPeriod(INACTIVITY_PERIOD_SECONDS); // 40 seconds; based on https://shorturl.at/JiZxK
  beam.setNewFileOnBoot(NEW_FILE_ON_BOOT);             // false to continue using same file if it's the same day
  beam.setLightGain(VEML7700_GAIN_2);
  beam.setLightIntegrationTime(VEML7700_IT_800MS);
  beam.logData();

  // Check if interval has passed (and set up alarm on first run)
  if (beam.alarm(SYNC_EVERY_MINUTES))
  {
    Serial.println("Alarm triggered!");
    // force sync (using meta.json beam settings)
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

// never enters loop()
void loop()
{
}

void beginHublink()
{
  if (hublink.begin())
  {
    Serial.println("✓ Hublink.");
    hublink.setTimestampCallback(onTimestampReceived);

    // override default values with values from meta.json
    if (hublink.hasMetaKey("beam", "log_every_minutes"))
    {
      LOG_EVERY_MINUTES = hublink.getMeta<int>("beam", "log_every_minutes");
      Serial.println("LOG_EVERY_MINUTES: " + String(LOG_EVERY_MINUTES));
    }
    if (hublink.hasMetaKey("beam", "sync_every_minutes"))
    {
      SYNC_EVERY_MINUTES = hublink.getMeta<int>("beam", "sync_every_minutes");
      Serial.println("SYNC_EVERY_MINUTES: " + String(SYNC_EVERY_MINUTES));
    }
    if (hublink.hasMetaKey("beam", "sync_for_seconds"))
    {
      SYNC_FOR_SECONDS = hublink.getMeta<int>("beam", "sync_for_seconds");
      Serial.println("SYNC_FOR_SECONDS: " + String(SYNC_FOR_SECONDS));
    }
    if (hublink.hasMetaKey("beam", "new_file_on_boot"))
    {
      NEW_FILE_ON_BOOT = hublink.getMeta<bool>("beam", "new_file_on_boot");
      Serial.println("NEW_FILE_ON_BOOT: " + String(NEW_FILE_ON_BOOT));
    }
    if (hublink.hasMetaKey("beam", "inactivity_period_seconds"))
    {
      INACTIVITY_PERIOD_SECONDS = hublink.getMeta<int>("beam", "inactivity_period_seconds");
      Serial.println("INACTIVITY_PERIOD_SECONDS: " + String(INACTIVITY_PERIOD_SECONDS));
    }
    if (hublink.hasMetaKey("beam", "randomize_alarm_minutes"))
    {
      RANDOMIZE_ALARM_MINUTES = hublink.getMeta<int>("beam", "randomize_alarm_minutes");
      Serial.println("RANDOMIZE_ALARM_MINUTES: " + String(RANDOMIZE_ALARM_MINUTES));
    }
    if (hublink.hasMetaKey("device", "id"))
    {
      DEVICE_ID = hublink.getMeta<String>("device", "id");
      Serial.println("DEVICE_ID: " + String(DEVICE_ID));
    }

    // Set device ID in beam library
    beam.setDeviceID(DEVICE_ID);
    beam.setAlarmRandomization(RANDOMIZE_ALARM_MINUTES);
    hublink.setBatteryLevel(round(beam.getBatteryPercent())); // send battery level to gateway
  }
  else
  {
    Serial.println("✗ Hublink Failed.");
    beam.setNeoPixel(NEOPIXEL_RED);
  }
}

void syncUnlessSwitchBDown()
{
  if (!beam.switchBDown())
  {
    bool didSync = false;
    Serial.println("Switch B not pressed - entering force sync mode");

    while (!didSync && !beam.switchBDown())
    {
      Serial.println("Starting sync...");
      beam.setNeoPixel(NEOPIXEL_RED);
      didSync = hublink.sync(SYNC_FOR_SECONDS);
      beam.setNeoPixel(NEOPIXEL_OFF);
      Serial.printf("Sync %s\n", didSync ? "successful" : "failed");

      if (!didSync && !beam.switchBDown())
      {
        delay(1000); // Wait before retry (but check switch during delay too)
        // Check switch state during the delay period
        for (int i = 0; i < 10 && !beam.switchBDown(); i++)
        {
          delay(100);
        }
      }
    }

    if (didSync)
    {
      // Brief white flash to indicate successful sync
      Serial.println("Sync complete - exiting sync mode");
      beam.setNeoPixel(NEOPIXEL_WHITE);
      delay(200);
      beam.setNeoPixel(NEOPIXEL_OFF);
    }
    else
    {
      // Switch was pressed during sync attempts
      Serial.println("Switch B pressed - aborting sync attempts");
    }
  }
  else
  {
    Serial.println("Switch B pressed - skipping sync");
  }
}