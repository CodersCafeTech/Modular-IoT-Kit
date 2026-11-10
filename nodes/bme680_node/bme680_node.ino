// BME680 environment node - XIAO ESP32-C3
// Wakes every SLEEP_SECONDS, reads the BME680, sends four readings to the gateway, sleeps.
//
// Sends: TEMPERATURE:<degC>  HUMIDITY:<%RH>  PRESSURE:<hPa>  GAS:<kOhm>
// Wiring: BME680 SDA -> D4 (GPIO6), SCL -> D5 (GPIO7), VCC -> 3V3, GND -> GND

#include <Wire.h>
#include <Adafruit_BME680.h>
#include <esp_sleep.h>
#include "ble_link.h"

// ---- Node settings ----
constexpr int I2C_SDA_PIN = D4;
constexpr int I2C_SCL_PIN = D5;
constexpr uint8_t BME_ADDRESSES[] = {0x77, 0x76};  // module-dependent; both are tried
constexpr uint64_t SLEEP_SECONDS = 60;
constexpr uint32_t SCAN_SECONDS = 5;
constexpr int CONNECT_ATTEMPTS = 3;
constexpr uint16_t GAS_HEATER_TEMP_C = 320;
constexpr uint16_t GAS_HEATER_MS = 150;
constexpr uint32_t SERIAL_SETTLE_MS = 500;

Adafruit_BME680 bme(&Wire);

struct Reading {
  float temperatureC;
  float humidityPct;
  float pressureHpa;
  float gasKohm;
};

void sleepUntilNextReading() {
  Serial.printf("Sleeping %llu s\n", SLEEP_SECONDS);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

bool beginSensor() {
  for (uint8_t address : BME_ADDRESSES) {
    if (bme.begin(address)) {
      Serial.printf("BME680 found at 0x%02X\n", address);
      bme.setTemperatureOversampling(BME680_OS_8X);
      bme.setHumidityOversampling(BME680_OS_2X);
      bme.setPressureOversampling(BME680_OS_4X);
      bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
      bme.setGasHeater(GAS_HEATER_TEMP_C, GAS_HEATER_MS);
      return true;
    }
  }
  return false;
}

bool readSensor(Reading &out) {
  if (!bme.performReading()) {
    return false;
  }
  out = Reading{
      bme.temperature,
      bme.humidity,
      bme.pressure / 100.0f,
      bme.gas_resistance / 1000.0f,
  };
  return true;
}

void sendReading(const Reading &r) {
  ble_link::send("TEMPERATURE:" + String(r.temperatureC, 2));
  ble_link::send("HUMIDITY:" + String(r.humidityPct, 2));
  ble_link::send("PRESSURE:" + String(r.pressureHpa, 2));
  ble_link::send("GAS:" + String(r.gasKohm, 2));
}

void setup() {
  Serial.begin(115200);
  delay(SERIAL_SETTLE_MS);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  if (!beginSensor()) {
    Serial.println("BME680 not found - check wiring/address. Retrying after sleep.");
    sleepUntilNextReading();
  }

  Reading reading;
  if (!readSensor(reading)) {
    Serial.println("BME680 reading failed. Retrying after sleep.");
    sleepUntilNextReading();
  }
  Serial.printf("T=%.2fC RH=%.2f%% P=%.2fhPa Gas=%.2fkOhm\n", reading.temperatureC,
                reading.humidityPct, reading.pressureHpa, reading.gasKohm);

  ble_link::begin();
  if (ble_link::connectWithRetry(SCAN_SECONDS, CONNECT_ATTEMPTS)) {
    sendReading(reading);
    ble_link::disconnect();
  } else {
    Serial.println("Gateway unreachable, reading skipped.");
  }
  sleepUntilNextReading();
}

void loop() {
  // Never reached: setup() always ends in deep sleep.
}
