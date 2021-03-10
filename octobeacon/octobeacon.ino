#include <algorithm>
#include <Arduino.h>
#include <BLEAdvertising.h>
#include <BLEBeacon.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>
#include <sstream>
#include <string>
#include <Ticker.h>
#include <utility>
using namespace std;

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

// Device Name: Maximum 29 bytes
#define DEVICE_NAME "Octobeacon"

// UUID Generator: https://www.uuidgenerator.net/version4
#define UUID_SERVICE "89798198-0000-0000-0000-000000000000"
#define UUID_CHARACTERISTIC "89798198-249d-48bd-894e-db156f1a70e6"

#define PIN_BUTTON 0
#define STORAGE_NAMESPACE "linebeacon"
#define VERSION 1

#pragma pack(push, 1)
struct LineBeacon {
  uint64_t offset; // timestamp 偏移量
  uint8_t hwid[5];
  uint8_t vendor[4];
  uint8_t lot[8];
};

struct LineBeaconSetting {
  uint8_t version;
  uint8_t enabled; // 標記哪些 beacon 啟用
  uint64_t ts; // timestamp
  LineBeacon beacons[8]; // 此版本允許有 8 個 LINE Beacon
};

struct NvsSetting {
  uint8_t enabled; // 標記哪些 beacon 啟用
  LineBeacon beacons[8]; // 此版本允許有 8 個 LINE Beacon
};
#pragma pack(pop)

BLEAdvertisementData adDataFactory, adDataIbeacon, adDataLine;
BLEAdvertising *pAdvertising;
BLECharacteristic *pCharacteristic;
BLESecurity *pSecurity;
BLEServer *pServer;
BLEService *pService;
LineBeacon beacons[8];
LineBeaconSetting setting; // 儲存新舊紀錄
NvsSetting nvsSetting;
Ticker tickerNextBeacon, tickerTimestamp;
uint32_t beaconCur = 0, beaconLen = 0, factoryMode = 0;
volatile int interruptButton = 0;

void setup () {
  Serial.begin(115200);

  setupButton(); // 設定按鈕

  nvsReadSetting(); // 從 nvs 中讀取 setting
  nvsReadTs(); // 從 nvs 中讀取 timestamp
  setupLineBeacons(); // 從 setting 中解析 beacons
  setupAdDataIbeacon();
  setupAdDataFactory();
  setupBLE(); // 設定 BLE

  startNormalMode(); // 啟動一般模式
}

void loop () {
  if (interruptButton) {
    interruptButton = 0;
    factoryMode = 1 - factoryMode;
    if (!factoryMode) {
      // Normal Mode
      stopFactoryMode();
      startNormalMode();
    } else {
      // Factory Mode
      stopNormalMode();
      startFactoryMode();
    }
    Serial.println(("Beacon Mode: " + to_string(factoryMode)).c_str());
  }
}

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
    Serial.println(("MyServerCallbacks::onConnect(), conn_id = " + to_string(param->connect.conn_id)).c_str());
  };

  void onDisconnect(BLEServer* pServer) {
    Serial.println("MyServerCallbacks::onDisconnect()");
    delay(500); // 等候 BLE Stack 準備完成
    BLEDevice::startAdvertising(); // 重新開始廣播
    Serial.println("pAdvertising restarted.");
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onRead (BLECharacteristic *pCharacteristic) {
    Serial.println("onRead");
    if (factoryMode) {
      pCharacteristic->setValue((uint8_t*) &setting, sizeof(LineBeaconSetting));
    } else {
      pCharacteristic->setValue("");
    }
  }

  void onWrite (BLECharacteristic *pCharacteristic) {
    if (!factoryMode) {
      Serial.println("Not in factory mode.");
      return;
    }
    string newValue = pCharacteristic->getValue();
    if (newValue.length() != sizeof(LineBeaconSetting)) {
      Serial.println(("setting.length() should be " + to_string(sizeof(LineBeaconSetting)) + " bytes, received " + to_string(newValue.length()) + " bytes.").c_str());
      return;
    }
    // set new value to new setting
    if (newValue[0] != VERSION) {
      Serial.println(("setting.version should be " + to_string(VERSION) + ", received " + to_string(setting.version) + ".").c_str());
      return;
    }
    memcpy(&setting, newValue.c_str(), sizeof(LineBeaconSetting));
    dumpSetting(setting);
    nvsWriteSetting();
    nvsWriteTs();
    setupLineBeacons();
  }
};

void setupButton () {
  pinMode(PIN_BUTTON, INPUT_PULLUP); // INPUT_PULLUP: 0=HIGH, 1=LOW
  attachInterrupt(PIN_BUTTON, onButtonUp, RISING);
}

void nvsReadSetting () {
  Preferences storage;
  storage.begin(STORAGE_NAMESPACE);
  size_t len = storage.getBytesLength("setting");
  if (len == sizeof(NvsSetting)) {
    storage.getBytes("setting", (char *) &nvsSetting, len);
    setting.enabled = nvsSetting.enabled;
    for(int i=0; i<8; i++) setting.beacons[i] = nvsSetting.beacons[i];
  } else {
    storage.remove("setting");
    memset(&setting, '\0', sizeof(LineBeaconSetting));
  }
  setting.version = VERSION;
}

void nvsWriteSetting () {
  nvsSetting.enabled = setting.enabled;
  for(int i=0; i<8; i++) nvsSetting.beacons[i] = setting.beacons[i];

  Preferences storage;
  storage.begin(STORAGE_NAMESPACE);
  storage.putBytes("setting", &nvsSetting, sizeof(NvsSetting));
}

void nvsReadTs () {
  Preferences storage;
  storage.begin(STORAGE_NAMESPACE);
  setting.ts = storage.getULong64("ts", 0);
}

void nvsWriteTs () {
  Preferences storage;
  storage.begin(STORAGE_NAMESPACE);
  storage.putULong64("ts", setting.ts);
}

void setupLineBeacons () {
  beaconLen = 0;
  beaconCur = 0;
  for (int i=0; i<8; i++) {
    if (!((setting.enabled >> (7 - i)) & 1)) continue;
    beacons[beaconLen++] = setting.beacons[i];
  }
  if (beaconLen == 0) { // 沒有任何 beacon 啟用時，建立一個空的
    beaconLen = 1;
    memset(&beacons, '\0', sizeof(LineBeacon));
  }
  setupAdDataLine();
}

void setupAdDataLine () {
  BLEAdvertisementData adDataTmp;
  BLEUUID uuid("FE6F");
  adDataTmp.setFlags(0x06); // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  adDataTmp.setCompleteServices(uuid); // LINE Corp UUID

  LineBeacon &bc = beacons[beaconCur / 2];
  uint64_t ts = setting.ts + bc.offset; // timestamp
  string battery = "\x01";
  string payload = string((char *) &ts, 8) + string((char *) &bc.hwid, 17) + battery;
  for (int i=0; i<4; i++) swap(payload[i], payload[7 - i]); // timestamp need reverse
  string hash = sha256(payload);
  for (int i=0; i<4; i++) {
    for (int j=1; j<8; j++) hash[i] ^= hash[i + 4*j];
  }

  adDataTmp.setServiceData(uuid, "\x02" + string((char *) &bc.hwid, 5) + "\x7f" + hash.substr(0, 4) + payload.substr(6, 2) + battery);
  adDataLine = adDataTmp;
}

void setupAdDataIbeacon () {
  BLEAdvertisementData adDataTmp;
  adDataTmp.setFlags(0x04); // BR_EDR_NOT_SUPPORTED 0x04

  BLEBeacon beacon;
  beacon.setManufacturerId(0x4C00);
  beacon.setProximityUUID(BLEUUID((uint8_t *) hex2str("d0d2ce249efc11e582c41c6a7a17ef38").c_str(), 16, false));
  beacon.setMajor(0x4C49);
  beacon.setMinor(0x4e45);
  adDataTmp.addData("\x1a\xff" + beacon.getData()); // len = 0x1a = 26

  adDataIbeacon = adDataTmp;
}

void setupAdDataFactory () {
  BLEAdvertisementData adDataTmp;
  adDataTmp.setFlags(0x06); // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  string uuid((char *)(BLEUUID(UUID_SERVICE).to128().getNative()->uuid.uuid128), 16);
  adDataTmp.addData("\x11\x07" + uuid);
  adDataFactory = adDataTmp;
}

void setupBLE () {
  // 設定 BLEDevice
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, POWER_LEVEL);

  // 設定伺服器安全性
  pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  // 建立 GATT 伺服器
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // 設定自訂服務
  pService = pServer->createService(UUID_SERVICE);
  pCharacteristic = pService->createCharacteristic(UUID_CHARACTERISTIC, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();

  // 設定廣播內容
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(UUID_SERVICE);
  pAdvertising->setAdvertisementData(adDataIbeacon); // 預設 iBeacon

  // 透過網卡號碼計算一個辨識用的名稱
  string addr((char *) BLEDevice::getAddress().getNative(), 6);
  for (int i=1; i<3; i++) {
    addr[0] ^= addr[i*2];
    addr[1] ^= addr[i*2 + 1];
  }
  string deviceName = string(DEVICE_NAME) + " " + str2hex(addr.substr(0, 2));

  // 設定 scan 的廣播內容
  BLEAdvertisementData adDataScan;
  adDataScan.setName(deviceName);
  esp_ble_gap_set_device_name(deviceName.c_str());
  pAdvertising->setScanResponseData(adDataScan);
  pAdvertising->setMinPreferred(0x06); //slave connection min interval, Time = min_interval * 1.25 msec
  pAdvertising->setMinPreferred(0x10); //slave connection max interval, Time = max_interval * 1.25 msec
  BLEDevice::startAdvertising();
}

void startNormalMode () {
  Serial.println("startNormalMode()");

  beaconCur = 0;
  pAdvertising->setAdvertisementData((beaconCur & 1) ? adDataLine : adDataIbeacon);
  tickerTimestamp.attach(16.0, onTickerTimestamp);
  tickerNextBeacon.attach(0.5, onTickerNextBeacon);
}

void stopNormalMode () {
  Serial.println("stopNormalMode()");
  tickerTimestamp.detach();
  tickerNextBeacon.detach();
}

void startFactoryMode () {
  Serial.println("startFactoryMode()");
  pAdvertising->setAdvertisementData(adDataFactory);
}

void stopFactoryMode () {
  Serial.println("stopFactoryMode()");
}

uint8_t htoi (uint8_t c) {
  if ('0' <= c && c <= '9') return c - '0';
  if ('A' <= c && c <= 'F') return c - 'A' + 10;
  if ('a' <= c && c <= 'f') return c - 'a' + 10;
  return 0;
}

string hex2str (string hex) {
  string str(hex.length() / 2, '\0');
  for (int i=0; i<str.length(); i++) str[i] = (htoi(hex[i*2]) << 4) + htoi(hex[i*2+1]);
  return str;
}

string str2hex (const void * str, size_t len) {
  auto cstr = (const uint8_t *) str;
  const char *digits = "0123456789abcdef";
  string hex(len * 2, '0');
  for (int i=0; i<len; i++) {
    hex[i*2] = digits[cstr[i] >> 4];
    hex[i*2+1] = digits[cstr[i] & 0xF];
  }
  return hex;
}

string str2hex (string str) {
  return str2hex((const uint8_t *) str.c_str(), str.length());
}

string str2hex_reverse (const void * str, size_t len) {
  auto cstr = (const uint8_t *) str;
  const char *digits = "0123456789abcdef";
  string hex(len * 2, '0');
  for (int i=0; i<len; i++) {
    hex[i*2] = digits[cstr[len-i-1] >> 4];
    hex[i*2+1] = digits[cstr[len-i-1] & 0xF];
  }
  return hex;
}

string str2hex_reverse (string str) {
  return str2hex_reverse((const uint8_t *) str.c_str(), str.length());
}

string str2bin (const uint8_t *cstr, size_t len) {
  string bin(len * 8, '0');
  for (int i=0; i<len; i++) {
    for (int j=0; j<8; j++) bin[8 * i + j] = (cstr[i] & (1 << (7 - j))) ? '1' : '0';
  }
  return bin;
}

void onButtonUp () {
  interruptButton = 1;
}

template<class T> string to_string(T val) {
  ostringstream tmp;
  tmp << val;
  return tmp.str();
}

void onTickerTimestamp () {
  if (factoryMode || !setting.enabled) return;
  setting.ts++;
  nvsWriteTs();
  setupAdDataLine();
  pAdvertising->setAdvertisementData((beaconCur & 1) ? adDataLine : adDataIbeacon);
}

void onTickerNextBeacon () {
  uint32_t tmp = (beaconCur >= beaconLen * 2 - 1) ? 0 : beaconCur + 1;
  if (tmp / 2 != beaconCur / 2) setupAdDataLine();
  beaconCur = tmp;
  pAdvertising->setAdvertisementData((beaconCur & 1) ? adDataLine : adDataIbeacon);
}

string sha256 (string payload) {
  char hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0); // not sha224
  mbedtls_sha256_update_ret(&ctx, (uint8_t *) payload.c_str(), payload.length());
  mbedtls_sha256_finish_ret(&ctx, (uint8_t *) hash);
  mbedtls_sha256_free(&ctx);
  return string(hash, 32);
}

void dumpBeacon (string name, LineBeacon &t) {
  Serial.println((
    name +
    " offset = " + str2hex_reverse(&t.offset, 8) +
    ", hwid = " + str2hex(&t.hwid, sizeof(t.hwid)) +
    ", vendor = " + str2hex(&t.vendor, sizeof(t.vendor)) +
    ", lot = " + str2hex(&t.lot, sizeof(t.lot))
  ).c_str());
}

void dumpSetting (LineBeaconSetting &s) {
  Serial.println((
    "Setting: version = " + str2hex(&s.version, sizeof(s.version)) +
    ", enabled = " + str2bin(&s.enabled, sizeof(s.enabled)) +
    ", ts = " + str2hex_reverse(&s.ts, 8)
  ).c_str());
  for (int i=0; i<8; i++) dumpBeacon("  beacon[" + to_string(i) + "]:", s.beacons[i]);
}
