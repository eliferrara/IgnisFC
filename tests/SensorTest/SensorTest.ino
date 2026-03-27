/*
 * IgnisFC Sensor Test
 * 
 * Reads BMI270 (accel + gyro) and BMP580 (pressure + temp) over I2C,
 * prints data to USB serial, and blinks LED on GPIO21 once per second.
 *
 * Libraries required:
 *   arduino-cli lib install "SparkFun BMI270 Arduino Library"
 *   arduino-cli lib install "Adafruit BMP5xx Library"
 *   arduino-cli lib install "Adafruit Unified Sensor"
 *   arduino-cli lib install "Adafruit BusIO"
 *
 * Compile with:
 *   arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc IgnisFC
 *   arduino-cli upload  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc --port /dev/ttyACM0 IgnisFC
 */

#include <Wire.h>
#include <SparkFun_BMI270_Arduino_Library.h>
#include <Adafruit_BMP5xx.h>

// --- Pin Definitions ---
#define LED_PIN 21

// I2C pins — change these to match your schematic if not using defaults
#define I2C_SDA 8
#define I2C_SCL 9

// --- Sensor Objects ---
BMI270 imu;
Adafruit_BMP5xx bmp;

// --- LED Blink State ---
bool ledState = false;
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 500; // toggle every 500ms = 1Hz blink

// --- Sensor Read Timing ---
unsigned long lastSensorTime = 0;
const unsigned long SENSOR_INTERVAL = 500; // print every 500ms

bool bmi270_ok = false;
bool bmp580_ok = false;

void setup() {
  Serial.begin(115200);
  delay(1000); // give USB CDC time to connect

  Serial.println("=== IgnisFC Sensor Test ===");
  Serial.println();

  // LED setup
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // I2C setup
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400kHz fast mode

  // --- I2C Scanner ---
  Serial.println("Scanning I2C bus...");
  uint8_t devicesFound = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      devicesFound++;
    }
  }
  if (devicesFound == 0) {
    Serial.println("  No I2C devices found! Check wiring.");
  }
  Serial.println();

  // --- BMI270 Init ---
  Serial.print("Initializing BMI270... ");
  if (imu.beginI2C(BMI2_I2C_PRIM_ADDR, Wire) == BMI2_OK) {
    Serial.println("OK");
    bmi270_ok = true;
  } else {
    Serial.println("FAILED");
    Serial.println("  Trying alternate address 0x69...");
    if (imu.beginI2C(BMI2_I2C_SEC_ADDR, Wire) == BMI2_OK) {
      Serial.println("  OK at 0x69");
      bmi270_ok = true;
    } else {
      Serial.println("  FAILED - BMI270 not detected");
    }
  }

  // --- BMP580 Init ---
  Serial.print("Initializing BMP580... ");
  if (bmp.begin(BMP5XX_DEFAULT_ADDRESS, &Wire)) {
    Serial.println("OK");
    bmp580_ok = true;
  } else {
    Serial.println("FAILED at default address, trying alternate...");
    if (bmp.begin(BMP5XX_ALTERNATIVE_ADDRESS, &Wire)) {
      Serial.println("  OK at alternate address");
      bmp580_ok = true;
    } else {
      Serial.println("  FAILED - BMP580 not detected");
    }
  }

  if (bmp580_ok) {
    bmp.setTemperatureOversampling((bmp5xx_oversampling_t)BMP5_OVERSAMPLING_2X);
    bmp.setPressureOversampling((bmp5xx_oversampling_t)BMP5_OVERSAMPLING_16X);
    bmp.setIIRFilterCoeff((bmp5xx_iir_filter_t)BMP5_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate((bmp5xx_odr_t)BMP5_ODR_50_HZ);
  }

  Serial.println();
  Serial.println("--- Starting sensor loop ---");
  Serial.println("Accel(m/s2) X Y Z | Gyro(deg/s) X Y Z | Temp(C) | Press(hPa) | Alt(m)");
  Serial.println();
}

void loop() {
  unsigned long now = millis();

  // --- LED Blink ---
  if (now - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  // --- Sensor Read & Print ---
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = now;

    // BMI270
    if (bmi270_ok) {
      imu.getSensorData();

      Serial.print("A: ");
      Serial.print(imu.data.accelX, 2); Serial.print(", ");
      Serial.print(imu.data.accelY, 2); Serial.print(", ");
      Serial.print(imu.data.accelZ, 2);

      Serial.print(" | G: ");
      Serial.print(imu.data.gyroX, 2); Serial.print(", ");
      Serial.print(imu.data.gyroY, 2); Serial.print(", ");
      Serial.print(imu.data.gyroZ, 2);
    } else {
      Serial.print("A: ---, ---, --- | G: ---, ---, ---");
    }

    // BMP580
    if (bmp580_ok) {
      if (bmp.performReading()) {
        Serial.print(" | T: ");
        Serial.print(bmp.temperature, 1);
        Serial.print("C");

        Serial.print(" | P: ");
        Serial.print(bmp.pressure / 100.0, 2);
        Serial.print("hPa");

        Serial.print(" | Alt: ");
        Serial.print(bmp.readAltitude(1013.25), 1);
        Serial.print("m");
      } else {
        Serial.print(" | T: --- | P: --- | Alt: ---");
      }
    } else {
      Serial.print(" | T: --- | P: --- | Alt: ---");
    }

    Serial.println();
  }
}
