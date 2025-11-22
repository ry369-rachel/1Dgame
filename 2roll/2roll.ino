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
const int BUTTON_PIN = 4; // button to GND, we'll use INPUT_PULLUP
int prevButtonState = HIGH;
// second button
const int BUTTON_PIN2 = 5;
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
const int NUM_LEDS = 25; // number of LEDs per strip
Adafruit_NeoPixel ledLeft = Adafruit_NeoPixel(NUM_LEDS, LED_LEFT_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ledRight = Adafruit_NeoPixel(NUM_LEDS, LED_RIGHT_PIN, NEO_GRB + NEO_KHZ800);
int leftColorIndex = 0;  // 0=red,1=green,2=blue
int rightColorIndex = 0; // 0=blue,1=green,2=red

// helper: map angle to zone number per spec
int getZone(float angle) {
  // dead zone
  if (angle > -10.0 && angle < 10.0) return 0;
  // right tilt positive zones
  if (angle >= 30.0 && angle <= 50.0) return 1;
  if (angle > 50.0 && angle <= 70.0) return 2;
  if (angle > 70.0 && angle <= 90.0) return 3;
  if (angle > 90.0 && angle <= 110.0) return 4;
  if (angle > 110.0 && angle <= 130.0) return 5;
  if (angle > 130.0 && angle <= 150.0) return 6;
  // left tilt negative zones
  if (angle <= -30.0 && angle >= -50.0) return -1;
  if (angle < -50.0 && angle >= -70.0) return -2;
  if (angle < -70.0 && angle >= -90.0) return -3;
  if (angle < -90.0 && angle >= -110.0) return -4;
  if (angle < -110.0 && angle >= -130.0) return -5;
  if (angle < -130.0 && angle >= -150.0) return -6;
  return 0;
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
  // initial colors: left red, right blue
  for (int i = 0; i < NUM_LEDS; i++) {
    ledLeft.setPixelColor(i, ledLeft.Color(255, 0, 0));
  }
  ledLeft.show();
  for (int i = 0; i < NUM_LEDS; i++) {
    ledRight.setPixelColor(i, ledRight.Color(0, 0, 255));
  }
  ledRight.show();
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

  // --- Zone-based angle mapping ---
  int zone1 = getZone(smoothedAngle1);
  if (zone1 != lastZone1) {
    if (zone1 > 0) {
      Serial.print("MPU1 entered right zone "); Serial.println(zone1);
      Keyboard.write('D');
    } else if (zone1 < 0) {
      Serial.print("MPU1 entered left zone "); Serial.println(zone1);
      Keyboard.write('A');
    } else {
      Serial.println("MPU1 returned to dead zone");
    }
    lastZone1 = zone1;
  }

  int zone2 = getZone(smoothedAngle2);
  if (zone2 != lastZone2) {
    if (zone2 > 0) {
      Serial.print("MPU2 entered right zone "); Serial.println(zone2);
      Keyboard.write('L');
    } else if (zone2 < 0) {
      Serial.print("MPU2 entered left zone "); Serial.println(zone2);
      Keyboard.write('J');
    } else {
      Serial.println("MPU2 returned to dead zone");
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

  // --- Button handling: one-shot send 'r' when button transitions from HIGH -> LOW ---
  int buttonState = digitalRead(BUTTON_PIN);
  int buttonState2 = digitalRead(BUTTON_PIN2);

  unsigned long now = millis();

  // BUTTON 1 (D4) handling: detect first click and wait for double-click window before sending single 'r'
  if (buttonState == LOW && prevButtonState == HIGH) {
    // falling edge detected
    if ((now - lastBounce1) > DEBOUNCE_MS) {
      lastBounce1 = now;
      if (clickCount1 == 0) {
        // first click: record time and wait for possible second click
        clickCount1 = 1;
        lastPressTime1 = now;
        Serial.println("Button1 first click recorded");
      } else if (clickCount1 == 1 && (now - lastPressTime1) <= DOUBLE_CLICK_MS) {
        // second click within window -> double-click
        Serial.println("Button1 double-click -> sending 'S'");
        Keyboard.write('S');
        // cycle left color: red -> green -> blue
        leftColorIndex = (leftColorIndex + 1) % 3;
        uint32_t col;
        if (leftColorIndex == 0) col = ledLeft.Color(255, 0, 0);
        else if (leftColorIndex == 1) col = ledLeft.Color(0, 255, 0);
        else col = ledLeft.Color(0, 0, 255);
        for (int pi = 0; pi < NUM_LEDS; pi++) ledLeft.setPixelColor(pi, col);
        ledLeft.show();
        Serial.print("Left LED color index -> "); Serial.println(leftColorIndex);
        // reset
        clickCount1 = 0;
        lastPressTime1 = 0;
      }
    }
  }
  // if we've seen a single first click and the window expired, treat as single press
  if (clickCount1 == 1 && (now - lastPressTime1) > DOUBLE_CLICK_MS) {
    Serial.println("Button1 single-click timeout -> sending 'r'");
    Keyboard.write('r');
    clickCount1 = 0;
    lastPressTime1 = 0;
  }

  // BUTTON 2 (D5) handling: same pattern as Button1
  if (buttonState2 == LOW && prevButtonState2 == HIGH) {
    if ((now - lastBounce2) > DEBOUNCE_MS) {
      lastBounce2 = now;
      if (clickCount2 == 0) {
        clickCount2 = 1;
        lastPressTime2 = now;
        Serial.println("Button2 first click recorded");
      } else if (clickCount2 == 1 && (now - lastPressTime2) <= DOUBLE_CLICK_MS) {
        Serial.println("Button2 double-click -> sending 'K'");
        Keyboard.write('K');
        rightColorIndex = (rightColorIndex + 1) % 3; // blue -> green -> red
        uint32_t col2;
        if (rightColorIndex == 0) col2 = ledRight.Color(0, 0, 255);
        else if (rightColorIndex == 1) col2 = ledRight.Color(0, 255, 0);
        else col2 = ledRight.Color(255, 0, 0);
        for (int pi2 = 0; pi2 < NUM_LEDS; pi2++) ledRight.setPixelColor(pi2, col2);
        ledRight.show();
        Serial.print("Right LED color index -> "); Serial.println(rightColorIndex);
        clickCount2 = 0;
        lastPressTime2 = 0;
      }
    }
  }
  if (clickCount2 == 1 && (now - lastPressTime2) > DOUBLE_CLICK_MS) {
    Serial.println("Button2 single-click timeout -> sending 'r'");
    Keyboard.write('r');
    clickCount2 = 0;
    lastPressTime2 = 0;
  }

  prevButtonState = buttonState;
  prevButtonState2 = buttonState2;

  delay(50); // stability per spec
}