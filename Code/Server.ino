#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <WiFiManager.h>

WiFiClient wifi;
PubSubClient mqttClient(wifi);

const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttClientId = "myClientIDs";

// WiFi credentials
//const char* ssid = "DARKMATTER";
//const char* password = "8848191317";

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pRemoteCharacteristic = NULL; // New remote characteristic

bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define REMOTE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9" // Unique UUID for the remote characteristic

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    BLEDevice::startAdvertising();
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    Serial.print("Received message from BLE Client: ");
    Serial.println(value.c_str());

    String topic = "UID123";
    if (mqttClient.connected()) {
      if (mqttClient.publish(topic.c_str(), value.c_str())) {
        Serial.println("Publish message success");
      } else {
        Serial.println("Publish message failed");
      }
    } else {
      Serial.println("MQTT client not connected");
    }
  }
};

void setup() {
  Serial.begin(115200);
  
  WiFiManager wm;
  bool res = wm.autoConnect("ModularIoT");

  if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected)");
    }

  BLEDevice::init("ESP32");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE
  );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->setValue("CharUUID");

  // Create a new remote characteristic to receive data from MQTT
  pRemoteCharacteristic = pService->createCharacteristic(
    REMOTE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_INDICATE
  );
  pRemoteCharacteristic->addDescriptor(new BLE2902());
  pRemoteCharacteristic->setValue("RemoteChar UUID"); // Initialize with an empty value
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Characteristics defined! Now you can read and write them from your phone!");

  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(subscribeReceive);
  connectToMQTTServer();
}

void loop() {
  // Handle BLE server
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  if (mqttClient.connected()) {
    mqttClient.loop();
  } else {
    reconnectToMQTTServer();
  }
}

void connectToMQTTServer() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT server...");
    if (mqttClient.connect(mqttClientId)) {
      Serial.println("Connected");
      mqttClient.subscribe("UID123");
    } else {
      Serial.print("Failed, retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void reconnectToMQTTServer() {
  if (!mqttClient.connected()) {
    Serial.println("MQTT connection lost. Reconnecting...");
    connectToMQTTServer();
  }
}

void subscribeReceive(char* topic, byte* payload, unsigned int length) {
  // Convert the MQTT payload to a string
  String mqttMessage = String((char*)payload, length);
  std::string mqttMessageStr = mqttMessage.c_str();
  
  Serial.println(mqttMessage.c_str());
  
  // Set the value of the remote characteristic with the received MQTT message
  pRemoteCharacteristic->setValue(mqttMessageStr);
  pRemoteCharacteristic->notify();
  Serial.println("Sent MQTT message to remote BLE client: " + mqttMessage);
}
