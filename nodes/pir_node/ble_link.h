#pragma once
// Shared BLE client link for Modular IoT nodes (XIAO ESP32-C3).
// An identical copy lives in every node sketch folder; edit all copies together.
//
// Gateway exposes one service with two characteristics:
//   UPLINK   (node -> gateway): node writes "ID:value", gateway publishes it to MQTT.
//   DOWNLINK (gateway -> node): gateway notifies dashboard commands, e.g. "BUTTON1:1".

#include <BLEDevice.h>

namespace ble_link {

static const BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static const BLEUUID UPLINK_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static const BLEUUID DOWNLINK_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a9");

static constexpr uint16_t REQUESTED_MTU = 185;

using CommandHandler = void (*)(const String &);

namespace detail {
static BLEClient *client = nullptr;              // created once, reused across reconnects
static BLEAdvertisedDevice *gateway = nullptr;   // last gateway seen while scanning
static BLERemoteCharacteristic *uplink = nullptr;
static BLERemoteCharacteristic *downlink = nullptr;
static CommandHandler commandHandler = nullptr;
static volatile bool connected = false;

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) override {
    if (!device.haveServiceUUID() || !device.isAdvertisingService(SERVICE_UUID)) {
      return;
    }
    delete gateway;
    gateway = new BLEAdvertisedDevice(device);
    BLEDevice::getScan()->stop();
  }
};

class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient *) override {}
  void onDisconnect(BLEClient *) override {
    connected = false;
    Serial.println("[ble] disconnected from gateway");
  }
};

static void onNotify(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  if (commandHandler != nullptr) {
    commandHandler(String(reinterpret_cast<const char *>(data), length));
  }
}

static void forgetGateway() {
  delete gateway;
  gateway = nullptr;
}
}  // namespace detail

// Call once in setup().
inline void begin() {
  BLEDevice::init("");
  detail::client = BLEDevice::createClient();
  detail::client->setClientCallbacks(new detail::ClientCallbacks());

  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new detail::ScanCallbacks());
  scan->setInterval(1349);
  scan->setWindow(449);
  scan->setActiveScan(true);
}

// Register before connect() to receive dashboard commands on the downlink.
inline void onCommand(CommandHandler handler) { detail::commandHandler = handler; }

inline bool isConnected() { return detail::connected; }

// Scans (blocking, up to scanSeconds) if no gateway is known, then connects.
inline bool connect(uint32_t scanSeconds) {
  using namespace detail;
  if (connected) {
    return true;
  }
  if (gateway == nullptr) {
    Serial.println("[ble] scanning for gateway...");
    BLEDevice::getScan()->start(scanSeconds, false);
    BLEDevice::getScan()->clearResults();
  }
  if (gateway == nullptr) {
    Serial.println("[ble] gateway not found");
    return false;
  }

  Serial.printf("[ble] connecting to %s\n", gateway->getAddress().toString().c_str());
  if (!client->connect(gateway)) {
    Serial.println("[ble] connect failed");
    forgetGateway();  // rescan next time
    return false;
  }
  client->setMTU(REQUESTED_MTU);

  BLERemoteService *service = client->getService(SERVICE_UUID);
  uplink = (service != nullptr) ? service->getCharacteristic(UPLINK_UUID) : nullptr;
  downlink = (service != nullptr) ? service->getCharacteristic(DOWNLINK_UUID) : nullptr;
  if (uplink == nullptr || downlink == nullptr) {
    Serial.println("[ble] gateway service/characteristics missing");
    client->disconnect();
    forgetGateway();
    return false;
  }

  if (commandHandler != nullptr && downlink->canNotify()) {
    downlink->registerForNotify(onNotify);
  }
  connected = true;
  Serial.println("[ble] connected");
  return true;
}

// Tries connect() up to `attempts` times.
inline bool connectWithRetry(uint32_t scanSeconds, int attempts) {
  for (int i = 0; i < attempts; ++i) {
    if (connect(scanSeconds)) {
      return true;
    }
  }
  return false;
}

// Sends "ID:value" to the gateway. Write-with-response, so the gateway has it on return.
inline bool send(const String &message) {
  if (!detail::connected || detail::uplink == nullptr) {
    Serial.printf("[ble] not connected, dropped: %s\n", message.c_str());
    return false;
  }
  detail::uplink->writeValue((uint8_t *)message.c_str(), message.length(), true);
  Serial.printf("[ble] sent %s\n", message.c_str());
  return detail::connected;
}

inline void disconnect() {
  if (detail::connected && detail::client != nullptr) {
    detail::client->disconnect();
  }
}

}  // namespace ble_link
