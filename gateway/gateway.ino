// Modular IoT gateway - ESP32-S3
// BLE server for the nodes, bridged to MQTT over WiFi.
//   Node writes "ID:value" to UPLINK  -> published to MQTT topic KIT_ID
//   Dashboard publishes "ID:value"    -> notified to all nodes on DOWNLINK
// WiFi is configured through the "ModularIoT" setup hotspot (WiFiManager).

#include <atomic>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "config.h"

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define UPLINK_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DOWNLINK_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"

constexpr size_t MESSAGE_MAX = 128;
constexpr size_t UPLINK_QUEUE_LENGTH = 32;
constexpr size_t ECHO_FILTER_SIZE = 16;
constexpr uint16_t MQTT_BUFFER_SIZE = 512;

struct UplinkMessage {
  char text[MESSAGE_MAX];
};

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
String mqttClientId;
uint32_t lastMqttAttemptMs = 0;

QueueHandle_t uplinkQueue = nullptr;
std::atomic<uint32_t> droppedUplinks{0};
uint32_t reportedDrops = 0;

BLECharacteristic *downlinkChar = nullptr;

// ---- Echo filter ----
// The gateway subscribes to the same topic it publishes on (the dashboard sends commands
// there). Remember our own publishes so their echoes are not forwarded to the nodes.
String recentPublishes[ECHO_FILTER_SIZE];
size_t recentHead = 0;

void rememberPublished(const String &message) {
  recentPublishes[recentHead] = message;
  recentHead = (recentHead + 1) % ECHO_FILTER_SIZE;
}

bool consumeEcho(const String &message) {
  for (String &entry : recentPublishes) {
    if (entry.length() > 0 && entry == message) {
      entry = "";
      return true;
    }
  }
  return false;
}

// ---- BLE ----
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    Serial.println("[ble] node connected");
    BLEDevice::startAdvertising();  // keep accepting more nodes
  }
  void onDisconnect(BLEServer *) override {
    Serial.println("[ble] node disconnected");
    BLEDevice::startAdvertising();
  }
};

// Runs in the BLE task: only queue the message; MQTT is used from loop() only.
class UplinkCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    UplinkMessage message{};
    auto value = characteristic->getValue();
    strncpy(message.text, value.c_str(), MESSAGE_MAX - 1);
    if (xQueueSend(uplinkQueue, &message, 0) != pdTRUE) {
      droppedUplinks++;
    }
  }
};

BLECharacteristic *addCharacteristic(BLEService *service, const char *uuid) {
  BLECharacteristic *characteristic = service->createCharacteristic(
      uuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_NOTIFY);
  characteristic->addDescriptor(new BLE2902());
  return characteristic;
}

void setupBle() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);
  BLECharacteristic *uplinkChar = addCharacteristic(service, UPLINK_UUID);
  uplinkChar->setCallbacks(new UplinkCallbacks());
  downlinkChar = addCharacteristic(service, DOWNLINK_UUID);
  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(false);
  advertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("[ble] advertising");
}

// ---- WiFi / MQTT ----
void setupWifi() {
  WiFiManager manager;
  manager.setConfigPortalTimeout(SETUP_PORTAL_TIMEOUT_S);
  if (!manager.autoConnect(SETUP_AP_NAME)) {
    Serial.println("[wifi] not configured/connected - restarting");
    delay(1000);
    ESP.restart();
  }
  WiFi.setAutoReconnect(true);
  Serial.printf("[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
}

void onMqttMessage(char *, byte *payload, unsigned int length) {
  const String message(reinterpret_cast<const char *>(payload), length);
  if (consumeEcho(message)) {
    return;
  }
  Serial.printf("[mqtt] -> nodes: %s\n", message.c_str());
  downlinkChar->setValue((uint8_t *)message.c_str(), message.length());
  downlinkChar->notify();
}

void setupMqtt() {
  mqttClientId = String(MQTT_CLIENT_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setCallback(onMqttMessage);
}

// Non-blocking: one connect attempt every MQTT_RETRY_MS.
void maintainMqtt() {
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  if (WiFi.status() != WL_CONNECTED || millis() - lastMqttAttemptMs < MQTT_RETRY_MS) {
    return;
  }
  lastMqttAttemptMs = millis();
  Serial.printf("[mqtt] connecting to %s as %s...\n", MQTT_HOST, mqttClientId.c_str());
  if (mqtt.connect(mqttClientId.c_str())) {
    mqtt.subscribe(KIT_ID);
    Serial.printf("[mqtt] connected, topic %s\n", KIT_ID);
  } else {
    Serial.printf("[mqtt] failed (state %d)\n", mqtt.state());
  }
}

// Publishes queued node messages. While MQTT is down they wait in the queue.
void forwardUplinks() {
  UplinkMessage message;
  while (mqtt.connected() && xQueueReceive(uplinkQueue, &message, 0) == pdTRUE) {
    String text(message.text);
    text.trim();
    if (text.length() == 0) {
      continue;
    }
    if (mqtt.publish(KIT_ID, text.c_str())) {
      rememberPublished(text);
      Serial.printf("[mqtt] <- node: %s\n", text.c_str());
    } else {
      Serial.printf("[mqtt] publish failed: %s\n", text.c_str());
    }
  }
}

void reportDrops() {
  const uint32_t drops = droppedUplinks.load();
  if (drops != reportedDrops) {
    Serial.printf("[ble] uplink queue full, %lu messages dropped so far\n", (unsigned long)drops);
    reportedDrops = drops;
  }
}

void setup() {
  Serial.begin(115200);
  uplinkQueue = xQueueCreate(UPLINK_QUEUE_LENGTH, sizeof(UplinkMessage));
  if (uplinkQueue == nullptr) {
    Serial.println("Out of memory creating uplink queue - restarting");
    delay(1000);
    ESP.restart();
  }
  setupWifi();
  setupMqtt();
  setupBle();
}

void loop() {
  maintainMqtt();
  forwardUplinks();
  reportDrops();
  delay(10);
}
