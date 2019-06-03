# LINE Simple Beacon for ESP32

Arduino code for LINE Simple Beacon work with ESP32

## Requirements

* [Arduino IDE](https://www.arduino.cc/en/Main/Software)
* [ESP32-DevKitC](https://www.espressif.com/en/products/hardware/esp32-devkitc/overview)
* Micro-USB to USB Cable
* [Create a channel on the LINE Developers console](https://developers.line.me/en/docs/line-login/getting-started/)

## Installation

Please ensure you have Arduino IDE installed and the board **disconnected**.

1. Open Arduino IDE
2. Go into **Preferences**
3. Add `https://dl.espressif.com/dl/package_esp32_index.json` as an 'Additional Board Manager URL'
4. Go to **Boards Manager** from the Tools -> Board menu
5. Search for esp32 and install **esp32**.
6. Download and install the [CP2102 driver](https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers)

## Setup
1. Connect the **ESP32-DevKitC** board via Micro-USB cable to your computer
2. Go into Tools -> Board and select **ESP32 Dev Module** from the list
3. Under Tools -> Port select the appropriate device *ie. COM1, /dev/cu.SLAB_USBtoUART*
4. Test uploading to the board by uploading an empty sketch to make sure there are no issues.
5. [Issue LINE Simple Beacon Hardware ID](https://manager.line.biz/beacon/register)
    ![](https://i.imgur.com/wHCaMzJ.jpg)
    ![](https://i.imgur.com/lF2lGDO.jpg)
    ![](https://i.imgur.com/INgnfju.jpg)

> If you can not find your channel in new [LINE Official Account Manager Beacon Register](https://manager.line.biz/beacon/register), you can try to register at old [LINE@ Manager Beacon Register](https://admin-official.line.me/beacon/register).
> ![](https://user-images.githubusercontent.com/30001185/50584877-afe5a900-0ea4-11e9-9130-69c3c893a301.png)
> ![](https://user-images.githubusercontent.com/30001185/50584907-e3283800-0ea4-11e9-8f3f-6645e1797785.png)
> ![](https://user-images.githubusercontent.com/30001185/50584909-e7545580-0ea4-11e9-97f2-063cfb1bfd8d.png)

## Upload

1. From this repository, open **sample/sample.ino**
2. Change the `HWID` to your generated HWID.
3. [Adjust the signal length](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/bluetooth/controller_vhci.html?highlight=esp_ble_tx_power_set#_CPPv220esp_ble_tx_power_set20esp_ble_power_type_t17esp_power_level_t)
4. Upload and Enjoy!

## Reference

* https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLEBeacon.h
* https://github.com/linedevth/LINE-Simple-Beacon-ESP32
* https://github.com/godda/LINE_Simple_Beacon_ESP32
* https://medium.com/@chawanwitpoolsri/line-beacon-from-esp32-node32-lite-72c0fc7dc646
* https://engineering.linecorp.com/ja/blog/detail/149/
* https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/bluetooth/index.html

## LICENSE

MIT
