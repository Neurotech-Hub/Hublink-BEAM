#include <Hublink.h>
#include <HublinkBEAM.h>

#define DO_DEBUG 1  // Set to 1 to enable serial debugging delays

HublinkBEAM beam;
Hublink hublink(PIN_SD_CS);
const unsigned long LOG_EVERY_MINUTES = 5;      // Log every X minutes
const unsigned long SYNC_EVERY_MINUTES = 30;    // Sync every X minutes
const unsigned long SYNC_TIMEOUT_SECONDS = 20;  // Sync timeout in seconds

void setup() {
  Serial.begin(115200);
#if DO_DEBUG
  delay(1000);  // Wait for serial connection
#endif

  // Configure file creation behavior
  beam.newFileOnBoot = true;  // false to continue using same file if it's the same day

  // Wait for the beam to initialize, retry (likely due to SD card ejecting)
  while (!beam.begin()) {
    delay(1000);  // Wait 1 second before retrying
  }

  // Configure alarm after beam initialization
  beam.setAlarmForEvery(SYNC_EVERY_MINUTES);
}

void loop() {
  beam.logData();
#if DO_DEBUG
  delay(1000);  // Wait for serial output
#endif

  // Check if interval has passed
  if (beam.alarmForEvery()) {
    Serial.println("Alarm triggered!");

    // only begin when alarm is triggered (risks not catching error at setup though)
    if (hublink.begin()) {
      Serial.println("✓ Hublink.");
      hublink.sync(SYNC_TIMEOUT_SECONDS);
    } else {
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
  beam.sleep(LOG_EVERY_MINUTES);  // Sleep for LOG_EVERY_MINUTES minutes
}