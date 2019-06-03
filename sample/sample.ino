#include <string>
#include "BLEDevice.h"
#include "BLEAdvertising.h"

/**
 * Bluetooth TX power level(index), it's just a index corresponding to power(dbm).
 * * ESP_PWR_LVL_N12 (-12 dbm)
 * * ESP_PWR_LVL_N9  (-9 dbm)
 * * ESP_PWR_LVL_N6  (-6 dbm)
 * * ESP_PWR_LVL_N3  (-3 dbm)
 * * ESP_PWR_LVL_N0  ( 0 dbm)
 * * ESP_PWR_LVL_P3  (+3 dbm)
 * * ESP_PWR_LVL_P6  (+6 dbm)
 * * ESP_PWR_LVL_P9  (+9 dbm)
 */
#define POWER_LEVEL ESP_PWR_LVL_P9

#define HWID "0000000000"

BLEAdvertising *pAdvertising;

std::string hexEncode(std::string raw) {
  const char *hexMap = "0123456789abcdef";
  std::string hex = "";
  for(int i=0; i<raw.size(); i++) {
    hex += hexMap[(raw[i] >> 4) & 0x0F];
    hex += hexMap[raw[i] & 0x0F];
  }
  return hex;
}

int htoi (unsigned char c)
{
  if ('0' <= c && c <= '9') return c - '0';
  if ('A' <= c && c <= 'F') return c - 'A' + 10;
  if ('a' <= c && c <= 'f') return c - 'a' + 10;
  return 0;
}

std::string hexDecode(std::string hex) {
  if (hex.size() % 2) return "";
  std::string raw = "";
  for(int i=0; i<hex.size(); i+=2) {
    raw += (char) ( htoi(hex[i]) * 16 + htoi(hex[i+1]) );
  }
  return raw;
}

bool setLINEBeacon (std::string hwid, std::string msg) {
  // check hwid
  if (hwid.size() != 5 || msg.size() > 13) return false;

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  BLEAdvertisementData oScanResponseData = BLEAdvertisementData();
  BLEUUID line_uuid("FE6F");

  // flag
  // LE General Discoverable Mode (2)
  // BR/EDR Not Supported (4)
  oAdvertisementData.setFlags(0x06);

  // LINE Corp UUID
  oAdvertisementData.setCompleteServices(line_uuid);

  // Service Data
  std::string payload = "";
  payload += (char) 0x02; // Frame Type of the LINE Simple Beacon Frame
  payload += hwid; // HWID of LINE Simple Beacon
  payload += 0x7F; // Measured TxPower of the LINE Simple Beacon Frame
  payload += msg; // Device message of LINE Simple Beacon Frame
  oAdvertisementData.setServiceData(line_uuid, payload);

  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setScanResponseData(oScanResponseData);
  return true;
}

void setup() {
  // init
  Serial.begin(115200);
  BLEDevice::init("");
  Serial.println(BLEDevice::toString().c_str());
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, POWER_LEVEL);
  Serial.println(((std::string)"POWER LEVEL: " + char(POWER_LEVEL+'0')).c_str());
  pAdvertising = BLEDevice::getAdvertising();

  Serial.println(((std::string)"LINE HWID: " + HWID).c_str());
  setLINEBeacon(hexDecode(HWID), hexEncode(std::string((const char*) BLEDevice::getAddress().getNative(), 6)));
  
  // Start advertising
  pAdvertising->start();
  Serial.println("Advertizing started...");
}

void loop()
{
  // The underlying framework will advertise periodically.
  // we simply wait here.
  delay(3000);
}
