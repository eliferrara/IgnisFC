# IgnisFC Setup Guide

Getting a brand new IgnisFC board from bare PCB to running firmware on Arch Linux.

## Board Overview

The IgnisFC is a custom flight computer built around the ESP32-S3 (QFN56, revision v0.2). Key specs from `esptool flash-id`:

- **MCU:** ESP32-S3, Dual Core + LP Core, 240MHz
- **Flash:** 16MB (Manufacturer: GigaDevice, quad SPI, 3.3V)
- **PSRAM:** 2MB embedded (AP_3v3)
- **Wireless:** Wi-Fi, Bluetooth 5 (LE)
- **Crystal:** 40MHz
- **USB:** Native USB-Serial/JTAG (no external UART chip)
- **MAC:** Unique per unit (e.g. `24:58:7c:6d:ed:74`)

## Onboard Sensors & Peripherals

| Component | Type | Interface | I2C Address | Library |
|-----------|------|-----------|-------------|---------|
| BMI270 | 6-axis IMU (accel + gyro) | I2C | 0x68 (default) / 0x69 (alt) | `SparkFun BMI270 Arduino Library` |
| BMP580 | Barometric pressure + temp | I2C | 0x47 (default) / 0x46 (alt) | `Adafruit BMP5xx Library` |
| LED | Status indicator | GPIO | — | Built-in `digitalWrite` |
| SG90 Servo | Deployment / actuation | PWM | — | `ESP32Servo` |

## GPIO Pin Assignments

| Pin | Function |
|-----|----------|
| GPIO8 | I2C SDA |
| GPIO9 | I2C SCL |
| GPIO19 | USB D- |
| GPIO20 | USB D+ |
| GPIO21 | Status LED |
| GPIO42 | Servo signal |
| GPIO0 (IO0) | Boot button (10kΩ pull-up to 3V3, button pulls LOW) |
| EN | Reset button (10kΩ pull-up to 3V3, button pulls LOW) |

## Power Supply

The board uses a **MIC5219-3.3YM5-TR** LDO regulator, powered from the USB 5V rail (not battery) to produce 3.3V for the ESP32-S3 and sensors.

**Known issue:** The MIC5219 requires output capacitor ESR between 0.2Ω–5Ω for stability. The stock capacitors (CL10A105KB8NNNC 1µF ceramic, CL05B104KO5NNNC 100nF ceramic) have ESR far too low (~0.01Ω). This doesn't cause the 3-second bootloop (that's just blank flash), but should be addressed for reliable operation under load. Options:

- Add a 0.5–1Ω resistor in series with the output cap
- Replace with a 10µF tantalum capacitor
- Add a 47–100µF electrolytic in parallel

## Host Setup (Arch Linux)

### 1. Install tools

```bash
sudo pacman -Syu
sudo pacman -S arduino-cli esptool
```

If `pacman` gives 404 errors on every mirror, your package database is out of sync. Run `sudo pacman -Syu` first.


### 2. USB permissions (not always necessary)

Add yourself to the `uucp` group so you can access `/dev/ttyACM0` without sudo:


```bash
sudo usermod -a -G uucp $USER
```

Log out and back in for this to take effect.

Optionally, create a udev rule for the ESP32-S3:

```bash
echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666"' | \
  sudo tee /etc/udev/rules.d/99-esp32.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### 3. Install ESP32 board support

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

### 4. Install libraries

```bash
arduino-cli lib install "SparkFun BMI270 Arduino Library"
arduino-cli lib install "Adafruit BMP5xx Library"
arduino-cli lib install "Adafruit Unified Sensor"
arduino-cli lib install "Adafruit BusIO"
arduino-cli lib install "ESP32Servo"
```

## Verifying Hardware

### Check USB enumeration

Plug in the board via USB-C and run:

```bash
dmesg | tail -20
```

You should see:

```
usb X-X: New USB device found, idVendor=303a, idProduct=1001
usb X-X: Product: USB JTAG/serial debug unit
usb X-X: Manufacturer: Espressif
cdc_acm X-X:1.0: ttyACM0: USB ACM device
```

If the device connects and disconnects every ~3 seconds, that's normal for a board with blank flash — the watchdog timer (`TG0WDT_SYS_RST`) resets the chip when it can't find valid firmware.

### Check serial output from a blank board (tty may vary)

```bash
picocom -b 115200 /dev/ttyACM0
```
Note: you can also just use `cat /dev/ttyACM0`

Blank flash produces:

```
invalid header: 0xffffffff
ESP-ROM:esp32s3-20210327
rst:0x7 (TG0WDT_SYS_RST),boot:0x9 (SPI_FAST_FLASH_BOOT)
invalid header: 0xffffffff
```

This confirms the hardware is working — it just needs firmware.

### Verify esptool communication

Put the board in download mode (hold BOOT, press+release RESET, release BOOT), then:

```bash
esptool --port /dev/ttyACM0 --chip esp32s3 flash-id
```

Expected output includes chip type, flash size (16MB), and crystal frequency (40MHz).

## Building & Flashing Firmware

### FQBN string

The full board identifier with USB serial enabled is:

```
esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc
```

The `USBMode=hwcdc,CDCOnBoot=cdc` flags are **required** — without them, `Serial.print()` goes to UART0 (hardware TX/RX pins) instead of the USB port, and you'll see nothing on your terminal.

### Sketch directory structure

Arduino-cli expects the `.ino` file to live in a directory with the same name, but you can also pass the `.ino` file path directly:

```bash
# Either of these work:
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc MySketch/
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc MySketch.ino
```

### Compile

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc MySketch.ino
```

### Upload

If the board already has firmware with USB CDC enabled, you can upload directly:

```bash
arduino-cli upload --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc \
  --port /dev/ttyACM0 MySketch.ino
```

If the board has blank flash or is bootlooping, enter download mode manually first:

1. Hold the **BOOT** button (pulls IO0 LOW)
2. Press and release the **RESET** button (cycles EN LOW → HIGH)
3. Release the **BOOT** button
4. Run the upload command

### Monitor serial output

```bash
arduino-cli monitor --port /dev/ttyACM0 --config baudrate=115200
```

Press Ctrl+C to exit. Make sure to close the monitor before uploading new firmware, or you'll get a "Device or resource busy" error.

## Serial Communication Notes

- The ESP32-S3 uses its **native USB-Serial/JTAG** interface — there is no external USB-to-UART chip (like CH340 or CP2102)
- The device appeared as `/dev/ttyACM0` during my testing, but this may vary between systems
- `Serial.begin(115200)` maps to USB CDC **only** when compiled with `CDCOnBoot=cdc`
- Without that flag, use `USBSerial.begin(115200)` and `USBSerial.println()` explicitly
- Add a `delay(1000)` after `Serial.begin()` in `setup()` to give the USB CDC connection time to establish before printing

## Adafruit BMP5xx Library Note

The Adafruit BMP5xx library (v1.0.2) defines oversampling/filter/ODR values as `UINT8_C()` macros, but the setter functions expect enum types. You must cast:

```cpp
bmp.setTemperatureOversampling((bmp5xx_oversampling_t)BMP5_OVERSAMPLING_2X);
bmp.setPressureOversampling((bmp5xx_oversampling_t)BMP5_OVERSAMPLING_16X);
bmp.setIIRFilterCoeff((bmp5xx_iir_filter_t)BMP5_IIR_FILTER_COEFF_3);
bmp.setOutputDataRate((bmp5xx_odr_t)BMP5_ODR_50_HZ);
```

## Quick Reference

```bash
# Full setup from scratch (after pacman and arduino-cli are installed):
arduino-cli core install esp32:esp32
arduino-cli lib install "SparkFun BMI270 Arduino Library"
arduino-cli lib install "Adafruit BMP5xx Library"
arduino-cli lib install "Adafruit Unified Sensor"
arduino-cli lib install "Adafruit BusIO"
arduino-cli lib install "ESP32Servo"

# Compile, upload, and monitor:
arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc MySketch.ino
arduino-cli upload  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc --port /dev/ttyACM0 MySketch.ino
arduino-cli monitor --port /dev/ttyACM0 --config baudrate=115200
```

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Board not in `/dev` at all | USB cable is charge-only, or CC resistors missing | Try a known-good data cable; verify 5.1kΩ on CC1/CC2 to GND |
| Connects/disconnects every ~3s | Blank flash, watchdog reset | Flash firmware (this is normal for a new board) |
| `dmesg` shows device but no `/dev/ttyACM0` | Missing `cdc_acm` kernel module | `sudo modprobe cdc_acm` |
| "Device or resource busy" on upload | Serial monitor still open | Ctrl+C the monitor first, then upload |
| Serial monitor shows nothing | Compiled without `CDCOnBoot=cdc` | Recompile with full FQBN flags |
| BMP5xx won't compile (int to enum) | Library macro/enum mismatch | Cast values to the enum types (see above) |
| `pacman` 404 on all mirrors | Stale package database | Run `sudo pacman -Syu` |
