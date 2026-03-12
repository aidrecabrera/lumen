# REFERENCE FOR WIRING

Current firmware support:

- ESP32 development board
- BH1750 ambient light sensor [[3]](#references)
- DHT11 temperature and humidity sensor [[8]](#references)
- WS2812B / NeoPixel-compatible LED ring or strip [[10]](#references)

Planned next sensor:

- TCS34725 color / spectral sensor [[13]](#references)

Relay control is not part of the current firmware.

---

## 1. Pin map

These values come from `include/lumen_board_config.h`.

| Function | GPIO |
|---|---:|
| I2C SDA | 21 |
| I2C SCL | 22 |
| DHT11 data | 4 |
| LED data | 18 |

GPIO 21 (SDA) and GPIO 22 (SCL) are the default I2C pins for the ESP32 in the Arduino framework [[1]](#references)[[2]](#references). These defaults are set in software by the framework, not fixed in hardware. The ESP32 GPIO matrix allows remapping, but this firmware uses the defaults [[1]](#references).

If `lumen_board_config.h` changes, this file has to match it.

---

## 2. Voltage domains

The current design uses two rails.

| Rail | Use |
|---|---|
| 3.3 V | ESP32 logic, BH1750, DHT11 |
| 5 V | WS2812B / NeoPixel power |

The BH1750 operating voltage range is 2.4 V to 3.6 V [[3]](#references). The DHT11 operating voltage range is 3.3 V to 5.5 V [[8]](#references). Both are compatible with the ESP32 3.3 V rail. WS2812B LEDs require 5 V power [[10]](#references)[[11]](#references).

All grounds must be common:

- ESP32 GND
- BH1750 GND
- DHT11 GND
- LED supply GND
- LED GND

That common ground is required for stable LED data [[10]](#references).

---

## 3. Interface summary

| Device | Interface | Voltage | Notes |
|---|---|---|---|
| BH1750 | I2C [[3]](#references) | 3.3 V | Shared bus on GPIO21 and GPIO22 |
| DHT11 | Single-wire digital [[7]](#references)[[8]](#references) | 3.3 V | Data on GPIO4 |
| WS2812B / NeoPixel | Single-wire LED protocol [[10]](#references) | 5 V power | Data on GPIO18 |
| TCS34725 | I2C [[13]](#references) | 3.3 V | Planned, shares the same I2C bus |

---

## 4. Current wiring matrix

## 4.1 ESP32 to BH1750

| BH1750 pin | Connect to | Net |
|---|---|---|
| VCC | ESP32 3V3 | +3V3 |
| GND | ESP32 GND | GND |
| SDA | ESP32 GPIO21 | I2C_SDA |
| SCL | ESP32 GPIO22 | I2C_SCL |
| ADDR | GND recommended | Address select |

Recommended default:

- `ADDR -> GND` for address `0x23` [[4]](#references)[[6]](#references)

Alternative:

- `ADDR -> 3V3` for address `0x5C` [[4]](#references)[[6]](#references)

Do not leave ADDR floating. While a floating ADDR pin typically resolves to LOW (address `0x23`) [[5]](#references), tying it explicitly to GND or VCC is the more robust practice.

---

## 4.2 ESP32 to DHT11

| DHT11 pin | Connect to | Net |
|---|---|---|
| VCC | ESP32 3V3 | +3V3 |
| DATA | ESP32 GPIO4 | DHT11_DATA |
| GND | ESP32 GND | GND |

If you are using a bare DHT11 sensor, add a 10 kOhm pull-up resistor from DATA to VCC [[7]](#references)[[9]](#references).

If you are using the common 3-pin DHT11 module, that pull-up is usually already on the board [[9]](#references).

> **Note:** The DHT11 datasheet lists 3.3 V to 5.5 V as the valid supply range [[8]](#references). Some guides recommend 5 V for longer cable runs [[9]](#references). At 3.3 V, keep the sensor within about 1 meter of the ESP32 for reliable readings.

---

## 4.3 ESP32 to WS2812B / NeoPixel

| LED input | Connect to | Net |
|---|---|---|
| DIN | ESP32 GPIO18 through 330 Ohm resistor | LED_DATA |
| GND | External 5 V supply GND and ESP32 GND | GND |
| 5V / VIN | External regulated 5 V supply | +5V |

Support parts:

| Component | Value | Placement |
|---|---|---|
| Series resistor | 330 Ohm | In series with GPIO18 to DIN |
| Bulk capacitor | 1000 uF, 6.3 V or higher | Across LED 5V and GND near LED input |

A 220 to 470 Ohm series resistor on the data line is recommended to reduce noise and protect the first LED's data input [[10]](#references)[[12]](#references). This design uses 330 Ohm, which falls in the center of that range.

A large electrolytic capacitor (100 to 1000 uF, 6.3 V or higher) across the 5 V and GND lines at the point of power entry prevents initial inrush current from damaging the LEDs [[10]](#references). This design specifies 1000 uF as a conservative baseline.

Use an external regulated 5 V supply for the LEDs. Do not power the LED ring or strip from the ESP32 3.3 V pin. A single WS2812B LED can draw up to 60 mA at full white [[11]](#references), so even a small strip quickly exceeds the ESP32 regulator's capacity.

---

## 5. Full current wiring

| Net | Source | Destination(s) |
|---|---|---|
| +3V3 | ESP32 3V3 | BH1750 VCC, DHT11 VCC |
| GND | ESP32 GND | BH1750 GND, DHT11 GND, LED GND, PSU GND |
| I2C_SDA | ESP32 GPIO21 | BH1750 SDA |
| I2C_SCL | ESP32 GPIO22 | BH1750 SCL |
| DHT11_DATA | ESP32 GPIO4 | DHT11 DATA |
| LED_DATA | ESP32 GPIO18 through 330 Ohm | WS2812B DIN |
| +5V | External PSU | WS2812B 5V |

---

## 6. Recommended power topology

Preferred setup:

- ESP32 powered from USB
- BH1750 powered from ESP32 3.3 V [[3]](#references)[[6]](#references)
- DHT11 powered from ESP32 3.3 V [[8]](#references)
- LED ring or strip powered from external regulated 5 V [[10]](#references)[[11]](#references)
- all grounds tied together [[10]](#references)

This is the cleanest setup for the current build.

---

## 7. LED current budget

Worst-case WS2812B current is about:

- 60 mA per LED at full white, full brightness [[11]](#references)

For `LED_PIXEL_COUNT = 16`:

- worst-case current is about `16 x 60 mA = 960 mA`

Use a regulated 5 V supply rated for at least 1 A. 2 A is a better choice if you want margin [[10]](#references).

---

## 8. Planned next sensor

TCS34725 is not part of the current firmware yet, but the intended bus placement is already clear.

| TCS34725 pin | Connect to | Net |
|---|---|---|
| VCC / VIN | ESP32 3V3 | +3V3 |
| GND | ESP32 GND | GND |
| SDA | ESP32 GPIO21 | I2C_SDA |
| SCL | ESP32 GPIO22 | I2C_SCL |
| INT | optional | not required by first integration |
| LED | optional | depends on module |

BH1750 and TCS34725 share the same I2C bus. This works without conflict because the BH1750 defaults to address `0x23` [[4]](#references) and the TCS34725 uses a fixed address of `0x29` [[13]](#references)[[14]](#references).

---

## 9. Wiring contract summary

This is the current supported contract:

- SDA to GPIO21 [[1]](#references)[[2]](#references)
- SCL to GPIO22 [[1]](#references)[[2]](#references)
- DHT11 DATA to GPIO4
- LED DIN to GPIO18 through 330 Ohm [[10]](#references)
- BH1750 and DHT11 on 3.3 V [[3]](#references)[[8]](#references)
- LED on external 5 V [[10]](#references)[[11]](#references)
- all grounds common [[10]](#references)

If you wire to that contract, it matches the current firmware.

---

## 10. References

| Ref | Title |
|-----|-------|
| [1] | [ESP32 I2C — Arduino Core Documentation (Espressif)](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html) |
| [2] | [ESP32 I2C Communication: Set Pins, Multiple Bus Interfaces and Peripherals (Random Nerd Tutorials)](https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/) |
| [3] | [BH1750FVI Digital 16-bit Serial Output Type Ambient Light Sensor IC Datasheet (ROHM Semiconductor)](https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf) |
| [4] | [BH1750 Arduino Library — claws/BH1750 (GitHub)](https://github.com/claws/BH1750) |
| [5] | [ErriezBH1750 Arduino Library — Erriez/ErriezBH1750 (GitHub)](https://github.com/Erriez/ErriezBH1750) |
| [6] | [Adafruit BH1750 Ambient Light Sensor Guide (Adafruit Learning System)](https://learn.adafruit.com/adafruit-bh1750-ambient-light-sensor) |
| [7] | [DHT11, DHT22, and AM2302 Sensors (Adafruit Learning System)](https://learn.adafruit.com/dht) |
| [8] | [DHT11 Humidity and Temperature Sensor Technical Data Sheet (Mouser)](https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf) |
| [9] | [In-Depth: Interface DHT11 and DHT22 Sensors with Arduino (Last Minute Engineers)](https://lastminuteengineers.com/dht11-dht22-arduino-tutorial/) |
| [10] | [Adafruit NeoPixel Uberguide: Best Practices (Adafruit Learning System)](https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices) |
| [11] | [How to Control WS2812B Individually Addressable LEDs Using Arduino (Last Minute Engineers)](https://lastminuteengineers.com/ws2812b-arduino-tutorial/) |
| [12] | [Guide for WS2812B Addressable RGB LED Strip with Arduino (Random Nerd Tutorials)](https://randomnerdtutorials.com/guide-for-ws2812b-addressable-rgb-led-strip-with-arduino/) |
| [13] | [TCS34725 Color Light-to-Digital Converter Datasheet (ams/TAOS)](https://cdn-shop.adafruit.com/datasheets/TCS34725.pdf) |
| [14] | [TCS34725 I2C Color Sensor Wiki (DFRobot)](https://wiki.dfrobot.com/TCS34725_I2C_Color_Sensor_For_Arduino_SKU__SEN0212) |
| [15] | [ESP32 Pinout Reference: Which GPIO Pins Should You Use? (Random Nerd Tutorials)](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/) |