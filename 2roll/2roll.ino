#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Keyboard.h>

Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;

// Key state tracking so we press on enter-zone and release on exit
bool dPressed = false;
bool aPressed = false;
bool lPressed = false;
bool jPressed = false;

// Smoothing + hysteresis params to reduce jitter
float smoothedAngle1 = 0.0;
float smoothedAngle2 = 0.0;
bool smoothedInit1 = false;
bool smoothedInit2 = false;
const float SMOOTH_ALPHA = 0.25; // EMA alpha (0..1). Lower == more smoothing
const float ENTER_THRESH = 12.0; // degrees to enter a direction
const float EXIT_THRESH = 8.0;   // degrees to exit (hysteresis)

// Repeat/hold behavior: send repeated key events while tilt is held
int activeDir1 = 0; // -1 = A, 0 = none, 1 = D
int activeDir2 = 0; // -1 = J, 0 = none, 1 = L
unsigned long lastSent1 = 0;
unsigned long lastSent2 = 0;
const unsigned long REPEAT_INTERVAL = 160; // ms between repeated writes while holding

// Button for 'R' action (one-shot on press)
const int BUTTON_PIN = 4; // button to GND, we'll use INPUT_PULLUP
int prevButtonState = HIGH;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(2000);  // 给模块上电稳定时间


  Serial.println("Initializing MPU6050...");

  // Initialize Keyboard HID (Arduino Micro/Leonardo must be used)
  Keyboard.begin();

  if (!mpu1.begin(0x68)) {
    Serial.println("❌ Failed to find MPU6050 at 0x68");
  } else {
    Serial.println("✅ MPU6050 #1 (0x68) initialized");
  }

  if (!mpu2.begin(0x69)) {
    Serial.println("❌ Failed to find MPU6050 at 0x69");
  } else {
    Serial.println("✅ MPU6050 #2 (0x69) initialized");
  }

  // 设置量程
  mpu1.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu1.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu2.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu2.setGyroRange(MPU6050_RANGE_500_DEG);

  Serial.println("Setup complete.\n");

  // configure button pin with internal pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  sensors_event_t a1, g1, temp1;
  sensors_event_t a2, g2, temp2;

  mpu1.getEvent(&a1, &g1, &temp1);
  mpu2.getEvent(&a2, &g2, &temp2);

  float angle1 = atan2(a1.acceleration.y, a1.acceleration.z) * 180 / PI;
  float angle2 = atan2(a2.acceleration.y, a2.acceleration.z) * 180 / PI;

  // --- Exponential moving average smoothing ---
  if (!smoothedInit1) {
    smoothedAngle1 = angle1;
    smoothedInit1 = true;
  } else {
    smoothedAngle1 = SMOOTH_ALPHA * angle1 + (1.0 - SMOOTH_ALPHA) * smoothedAngle1;
  }
  if (!smoothedInit2) {
    smoothedAngle2 = angle2;
    smoothedInit2 = true;
  } else {
    smoothedAngle2 = SMOOTH_ALPHA * angle2 + (1.0 - SMOOTH_ALPHA) * smoothedAngle2;
  }

  Serial.print("Angle1 (0x68): ");
  Serial.print(angle1);
  Serial.print("°,   Angle2 (0x69): ");
  Serial.print(angle2);
  Serial.println("°");

  // --- Angle1 -> D / A (use smoothedAngle1 + hysteresis) ---
  // We'll send repeated Keyboard.write() events while the tilt remains in one direction
  int dir1 = 0;
  if (smoothedAngle1 > ENTER_THRESH) dir1 = 1;        // D
  else if (smoothedAngle1 < -ENTER_THRESH) dir1 = -1; // A

  if (dir1 != 0) {
    if (activeDir1 != dir1) {
      // newly entered a direction: send one key event immediately
      if (dir1 == 1) Keyboard.write('d'); else Keyboard.write('a');
      lastSent1 = millis();
      activeDir1 = dir1;
    } else {
      // still holding the same direction: send repeats at REPEAT_INTERVAL
      if (millis() - lastSent1 >= REPEAT_INTERVAL) {
        if (dir1 == 1) Keyboard.write('d'); else Keyboard.write('a');
        lastSent1 = millis();
      }
    }
  } else {
    // within neutral region: apply exit hysteresis to clear activeDir only when angle returns closer to 0
    if (activeDir1 == 1 && smoothedAngle1 < EXIT_THRESH) activeDir1 = 0;
    if (activeDir1 == -1 && smoothedAngle1 > -EXIT_THRESH) activeDir1 = 0;
  }

  // --- Angle2 -> L / J (use smoothedAngle2 + hysteresis) ---
  int dir2 = 0;
  if (smoothedAngle2 > ENTER_THRESH) dir2 = 1;        // L
  else if (smoothedAngle2 < -ENTER_THRESH) dir2 = -1; // J

  if (dir2 != 0) {
    if (activeDir2 != dir2) {
      if (dir2 == 1) Keyboard.write('l'); else Keyboard.write('j');
      lastSent2 = millis();
      activeDir2 = dir2;
    } else {
      if (millis() - lastSent2 >= REPEAT_INTERVAL) {
        if (dir2 == 1) Keyboard.write('l'); else Keyboard.write('j');
        lastSent2 = millis();
      }
    }
  } else {
    if (activeDir2 == 1 && smoothedAngle2 < EXIT_THRESH) activeDir2 = 0;
    if (activeDir2 == -1 && smoothedAngle2 > -EXIT_THRESH) activeDir2 = 0;
  }

  // Debug: show which keys are currently pressed
  Serial.print("Keys: ");
  if (dPressed) Serial.print("D ");
  if (aPressed) Serial.print("A ");
  if (lPressed) Serial.print("L ");
  if (jPressed) Serial.print("J ");
  if (!dPressed && !aPressed && !lPressed && !jPressed) Serial.print("(none)");
  Serial.println();

  // --- Button handling: one-shot send 'r' when button transitions from HIGH -> LOW ---
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW && prevButtonState == HIGH) {
    // button just pressed
    Keyboard.write('r');
  }
  prevButtonState = buttonState;

  delay(20); // faster response; adjust if needed
}