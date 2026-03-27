/*
 * IgnisFC Servo Test
 * 
 * Sweeps an SG90 servo on GPIO42 from 0 to 180 and back,
 * while blinking an LED on GPIO21 once per second.
 *
 * Compile with:
 *   arduino-cli compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc ServoTest
 *   arduino-cli upload  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc --port /dev/ttyACM0 ServoTest
 */

#include <ESP32Servo.h>

#define LED_PIN   21
#define SERVO_PIN 42

Servo servo;

bool ledState = false;
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 500;

int angle = 0;
int step = 1;
unsigned long lastServoTime = 0;
const unsigned long SERVO_INTERVAL = 15; // ~15ms per degree for smooth sweep

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== IgnisFC Servo Test ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400); // SG90 typical pulse range
  servo.write(0);

  Serial.println("Servo attached to GPIO42, sweeping 0-180");
}

void loop() {
  unsigned long now = millis();

  // LED blink
  if (now - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }

  // Servo sweep
  if (now - lastServoTime >= SERVO_INTERVAL) {
    lastServoTime = now;
    servo.write(angle);
    angle += step;
    if (angle >= 180) {
      angle = 180;
      step = -1;
    } else if (angle <= 0) {
      angle = 0;
      step = 1;
    }
  }
}
