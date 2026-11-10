// PIR motion node - XIAO ESP32-C3
// Deep-sleeps until the PIR output goes HIGH, sends "PIRVALUE:1", waits for the motion
// to end, sends "PIRVALUE:0", then sleeps again.
//
// Wiring (Schematic_PIR motion sensor): PIR OUT -> D3 (GPIO5), VDD -> 5V, GND -> GND
// The wake pin must be GPIO0-GPIO5 on the ESP32-C3.

#include <esp_sleep.h>
#include "ble_link.h"

// ---- Node settings ----
const char *const NODE_ID = "PIRVALUE";
constexpr int PIR_PIN = D3;
constexpr uint32_t SCAN_SECONDS = 5;
constexpr int CONNECT_ATTEMPTS = 3;
constexpr uint32_t MOTION_POLL_MS = 200;
constexpr uint32_t SERIAL_SETTLE_MS = 500;

void sleepUntilMotion() {
  Serial.println("Sleeping until motion");
  Serial.flush();
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIR_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
}

void sendState(int state) {
  if (!ble_link::connectWithRetry(SCAN_SECONDS, CONNECT_ATTEMPTS)) {
    Serial.printf("Gateway unreachable, %s:%d not sent\n", NODE_ID, state);
    return;
  }
  ble_link::send(String(NODE_ID) + ":" + String(state));
}

void waitForMotionToEnd() {
  while (digitalRead(PIR_PIN) == HIGH) {
    delay(MOTION_POLL_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_SETTLE_MS);
  pinMode(PIR_PIN, INPUT);

  if (digitalRead(PIR_PIN) == HIGH) {
    Serial.println("Motion detected");
    ble_link::begin();
    sendState(1);
    waitForMotionToEnd();
    Serial.println("Motion ended");
    sendState(0);
    ble_link::disconnect();
  }
  sleepUntilMotion();
}

void loop() {
  // Never reached: setup() always ends in deep sleep.
}
