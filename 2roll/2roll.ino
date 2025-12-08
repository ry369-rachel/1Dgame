#include <DFRobotDFPlayerMini.h>

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Keyboard.h>
#include <Adafruit_NeoPixel.h>

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
// Zone-based control: detect entering discrete angle zones
int lastZone1 = 0; // previous zone for MPU1
int lastZone2 = 0; // previous zone for MPU2

// Button for 'R' action (one-shot on press)
const int BUTTON_PIN = 5; // button to GND, we'll use INPUT_PULLUP
int prevButtonState = HIGH;
// second button
const int BUTTON_PIN2 = 6;
int prevButtonState2 = HIGH;

// double-click detection
unsigned long lastPressTime1 = 0;
unsigned long lastPressTime2 = 0;
int clickCount1 = 0;
int clickCount2 = 0;
const unsigned long DOUBLE_CLICK_MS = 400; // ms window for double-click

// debounce tracking
unsigned long lastBounce1 = 0;
unsigned long lastBounce2 = 0;
const unsigned long DEBOUNCE_MS = 50;

// NeoPixel LEDs on D9 and D10 (25 LEDs each)
#define LED_LEFT_PIN 9
#define LED_RIGHT_PIN 10
const int NUM_LEDS = 28; // number of LEDs per strip
Adafruit_NeoPixel ledLeft = Adafruit_NeoPixel(NUM_LEDS, LED_LEFT_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ledRight = Adafruit_NeoPixel(NUM_LEDS, LED_RIGHT_PIN, NEO_GRB + NEO_KHZ800);
int leftColorIndex = 0;  // 0=red,1=green,2=blue
int rightColorIndex = 0; // 0=blue,1=green,2=red

// helper: map angle to zone number for Player 1 (0-6, positive tilt)
// Zone 0: -10° to +10° (dead zone, grid position 1)
// Zone 1: +10° to +30° (grid position 2)
// Zone 2: +30° to +50° (grid position 3)
// Zone 3: +50° to +70° (grid position 4)
// Zone 4: +70° to +90° (grid position 5)
// Zone 5: +90° to +110° (grid position 6)
// Zone 6: +110° to +130° (grid position 7)
int getZone(float angle) {
  if (angle < -10.0) return 0;
  if (angle >= -10.0 && angle <= 10.0) return 0;
  if (angle > 10.0 && angle <= 30.0) return 1;
  if (angle > 30.0 && angle <= 50.0) return 2;
  if (angle > 50.0 && angle <= 70.0) return 3;
  if (angle > 70.0 && angle <= 90.0) return 4;
  if (angle > 90.0 && angle <= 110.0) return 5;
  if (angle > 110.0 && angle <= 130.0) return 6;
  if (angle > 130.0) return 6;
  return 0;
}

// helper: map angle to zone number for Player 2 (0-6, negative tilt mirror)
// Zone 0: +10° to -10° (dead zone, grid position 7/last) or any positive tilt beyond +10°
// Zone 1: -10° to -30° (grid position 6)
// Zone 2: -30° to -50° (grid position 5)
// Zone 3: -50° to -70° (grid position 4)
// Zone 4: -70° to -90° (grid position 3)
// Zone 5: -90° to -110° (grid position 2)
// Zone 6: -110° to -130° (grid position 1; < -130° also clamped to 6)
int getZoneRight(float angle) {
  // Positive tilts keep player2 at the last grid (zone 0)
  if (angle >= -10.0) return 0;
  if (angle >= -30.0 && angle < -10.0) return 1;
  if (angle >= -50.0 && angle < -30.0) return 2;
  if (angle >= -70.0 && angle < -50.0) return 3;
  if (angle >= -90.0 && angle < -70.0) return 4;
  if (angle >= -110.0 && angle < -90.0) return 5;
  if (angle >= -130.0 && angle < -110.0) return 6;
  // beyond -130° stays at zone 6
  if (angle < -130.0) return 6;
  return 0;
}

// Function to apply LED colors based on level
void applyLevelLedPalette(int lvl) {
  // level1: left red, right blue
  // level2: left green, right blue
  // level3: left red, right green
  uint32_t leftCol, rightCol;
  if (lvl == 1) {
    leftCol = ledLeft.Color(255, 0, 0);
    rightCol = ledRight.Color(0, 0, 255);
  } else if (lvl == 2) {
    leftCol = ledLeft.Color(0, 255, 0);
    rightCol = ledRight.Color(0, 0, 255);
  } else { // lvl == 3
    leftCol = ledLeft.Color(255, 0, 0);
    rightCol = ledRight.Color(0, 255, 0);
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    ledLeft.setPixelColor(i, leftCol);
    ledRight.setPixelColor(i, rightCol);
  }
  ledLeft.show();
  ledRight.show();
  Serial.print("Applied LED palette for level "); Serial.println(lvl);
}

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
  pinMode(BUTTON_PIN2, INPUT_PULLUP);

  // initialize LEDs
  ledLeft.begin();
  ledRight.begin();
  // set a comfortable default brightness (0-255)
  ledLeft.setBrightness(80);
  ledRight.setBrightness(80);
  // Apply Level 1 palette on startup
  applyLevelLedPalette(1);
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
  Serial.print(smoothedAngle1);
  Serial.print("°,   Angle2 (0x69): ");
  Serial.print(smoothedAngle2);
  Serial.println("°");

  // --- Zone-based angle mapping ---
  // Zone number directly corresponds to grid position (0-6)
  // Moving to higher zone -> D/J (forward)
  // Moving to lower zone -> A/L (backward)
  
  int zone1 = getZone(smoothedAngle1);
  if (zone1 != lastZone1) {
    if (zone1 > lastZone1) {
      // Zone increased: moving forward
      Serial.print("MPU1 zone "); Serial.print(lastZone1); Serial.print(" -> "); Serial.print(zone1); Serial.println(" (forward) -> D");
      Keyboard.write('D');
    } else if (zone1 < lastZone1) {
      // Zone decreased: moving backward
      Serial.print("MPU1 zone "); Serial.print(lastZone1); Serial.print(" -> "); Serial.print(zone1); Serial.println(" (backward) -> A");
      Keyboard.write('A');
    }
    lastZone1 = zone1;
  }

  int zone2 = getZoneRight(smoothedAngle2);
  if (zone2 != lastZone2) {
    if (zone2 > lastZone2) {
      // Zone increased: moving forward for Player 2 -> J
      Serial.print("MPU2 zone "); Serial.print(lastZone2); Serial.print(" -> "); Serial.print(zone2); Serial.println(" (forward) -> J");
      Keyboard.write('J');
    } else if (zone2 < lastZone2) {
      // Zone decreased: moving backward for Player 2 -> L
      Serial.print("MPU2 zone "); Serial.print(lastZone2); Serial.print(" -> "); Serial.print(zone2); Serial.println(" (backward) -> L");
      Keyboard.write('L');
    }
    lastZone2 = zone2;
  }

  // Debug: show which keys are currently pressed
  Serial.print("Keys: ");
  if (dPressed) Serial.print("D ");
  if (aPressed) Serial.print("A ");
  if (lPressed) Serial.print("L ");
  if (jPressed) Serial.print("J ");
  if (!dPressed && !aPressed && !lPressed && !jPressed) Serial.print("(none)");
  Serial.println();

  // --- Button handling: single-press locks color (send lock key) ---
  int buttonState = digitalRead(BUTTON_PIN);
  int buttonState2 = digitalRead(BUTTON_PIN2);

  unsigned long now = millis();

  // BUTTON 1 (D4): on falling edge, send lock key(s); do NOT change LEDs
  if (buttonState == LOW && prevButtonState == HIGH) {
    if ((now - lastBounce1) > DEBOUNCE_MS) {
      lastBounce1 = now;
      Serial.println("Button1 single-press -> sending lock key 'r' and 'R'");
      Keyboard.write('r');
      delay(10);
      Keyboard.write('R');
    }
  }

  // BUTTON 2 (D5): on falling edge, send lock key(s); do NOT change LEDs
  if (buttonState2 == LOW && prevButtonState2 == HIGH) {
    if ((now - lastBounce2) > DEBOUNCE_MS) {
      lastBounce2 = now;
      Serial.println("Button2 single-press -> sending lock key 'r' and 'R'");
      Keyboard.write('r');
      delay(10);
      Keyboard.write('R');
    }
  }

  prevButtonState = buttonState;
  prevButtonState2 = buttonState2;

  // process any incoming serial commands (e.g., level updates)
  processSerialCommands();

  delay(50); // stability per spec
}

// Serial command processing: accept 'L1','L2','L3' to set LED palettes per level
void processSerialCommands() {
  while (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) continue;
    Serial.print("Serial cmd: "); Serial.println(cmd);
    if (cmd.startsWith("L")) {
      int lvl = cmd.substring(1).toInt();
      if (lvl >= 1 && lvl <= 3) {
        applyLevelLedPalette(lvl);
      }
    }
  }
}