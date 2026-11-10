// Smart plug / relay node - XIAO ESP32-C3
// Stays connected to the gateway and switches the relay on dashboard commands
// "BUTTON1:1" (on) / "BUTTON1:0" (off). Messages for other IDs are ignored.
//
// Wiring: relay IN -> D2 (GPIO4), VCC/GND per relay module.

#include "ble_link.h"

// ---- Node settings ----
const char *const NODE_ID = "BUTTON1";
constexpr int RELAY_PIN = D2;
constexpr uint32_t SCAN_SECONDS = 5;
constexpr uint32_t RETRY_DELAY_MS = 2000;
constexpr uint32_t LOOP_DELAY_MS = 100;

// Runs in the BLE task; keep it short.
void handleCommand(const String &message) {
  const int separator = message.indexOf(':');
  if (separator < 0 || message.substring(0, separator) != NODE_ID) {
    return;  // not for this node (e.g. sensor traffic on the shared topic)
  }
  String value = message.substring(separator + 1);
  value.trim();
  if (value == "1") {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.printf("%s ON\n", NODE_ID);
  } else if (value == "0") {
    digitalWrite(RELAY_PIN, LOW);
    Serial.printf("%s OFF\n", NODE_ID);
  } else {
    Serial.printf("%s: invalid value '%s'\n", NODE_ID, value.c_str());
  }
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // relay off at power-up
  Serial.begin(115200);

  ble_link::begin();
  ble_link::onCommand(handleCommand);
}

void loop() {
  if (!ble_link::isConnected() && !ble_link::connect(SCAN_SECONDS)) {
    delay(RETRY_DELAY_MS);
  }
  delay(LOOP_DELAY_MS);
}
