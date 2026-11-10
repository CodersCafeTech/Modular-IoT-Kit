// Door (magnetic reed switch) node - XIAO ESP32-C3
// Door closed: magnet holds the reed switch closed -> pin HIGH.
// Deep-sleeps until the door opens (pin LOW), sends "REEDVALUE:1", waits for the door to
// close, sends "REEDVALUE:0", then sleeps again.
//
// Wiring (Schematic_Magnetic reed switch): reed switch between 3V3 and D1 (GPIO3),
// 10k pull-down from D1 to GND. Magnet on the door, node on the frame.
// The wake pin must be GPIO0-GPIO5 on the ESP32-C3.

#include <esp_sleep.h>
#include "ble_link.h"

// ---- Node settings ----
const char *const NODE_ID = "REEDVALUE";
constexpr int REED_PIN = D1;
constexpr int DOOR_OPEN_LEVEL = LOW;
constexpr uint32_t SCAN_SECONDS = 5;
constexpr int CONNECT_ATTEMPTS = 3;
constexpr uint32_t DOOR_POLL_MS = 200;
constexpr uint32_t SERIAL_SETTLE_MS = 500;

bool doorIsOpen() { return digitalRead(REED_PIN) == DOOR_OPEN_LEVEL; }

void sleepUntilDoorOpens() {
  Serial.println("Sleeping until door opens");
  Serial.flush();
  esp_deep_sleep_enable_gpio_wakeup(1ULL << REED_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void sendState(int state) {
  if (!ble_link::connectWithRetry(SCAN_SECONDS, CONNECT_ATTEMPTS)) {
    Serial.printf("Gateway unreachable, %s:%d not sent\n", NODE_ID, state);
    return;
  }
  ble_link::send(String(NODE_ID) + ":" + String(state));
}

void waitForDoorToClose() {
  while (doorIsOpen()) {
    delay(DOOR_POLL_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_SETTLE_MS);
  pinMode(REED_PIN, INPUT);  // external 10k pull-down

  if (doorIsOpen()) {
    Serial.println("Door opened");
    ble_link::begin();
    sendState(1);
    waitForDoorToClose();
    Serial.println("Door closed");
    sendState(0);
    ble_link::disconnect();
  }
  sleepUntilDoorOpens();
}

void loop() {
  // Never reached: setup() always ends in deep sleep.
}
