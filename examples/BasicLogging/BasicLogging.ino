#define DEBUG_SERIAL 0  // Set to 1 to enable debug delays

#include <HublinkBEAM.h>

HublinkBEAM beam;
const unsigned long LOG_INTERVAL = 60*10;  // Log every seconds

void setup() {
  Serial.begin(115200);
#if DEBUG_SERIAL
  delay(1000);
#endif

  // Wait for the beam to initialize, retry (likely due to SD card ejecting)
  while (!beam.begin()) {
    delay(1000);  // Wait 1 second before retrying
  }
}

void loop() {
#if DEBUG_SERIAL
  delay(500);
#endif

  beam.logData();

  /*
   * Motion logging requires calling sleep() to enable the ULP coprocessor monitoring.
   * The ULP program in ULPManager.cpp runs while the main processor is in deep sleep,
   * monitoring the PIR sensor's trigger pin (GPIO3/SDA). When motion is detected,
   * it sets a flag in RTC memory and halts until the next wake cycle. Note: Device
   * will restart after deep sleep, returning to setup(); nothing beyond beam.sleep()
   * will be executed.
   */
  beam.sleep(LOG_INTERVAL);
}