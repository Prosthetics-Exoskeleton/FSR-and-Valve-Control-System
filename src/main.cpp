#include <Arduino.h>

// Grip exoskeleton PCB v2 — absolute-position control.
//
// Change from v1: instead of acting on the *change* in flex (a motion/derivative
// controller with no target), we act on the *absolute* flex level:
//   - high reading  -> finger closed -> INFLATE muscle (inlet open, exhaust shut)
//   - low reading   -> finger open   -> DEFLATE muscle (exhaust open, inlet shut)
//   - in between     -> HOLD          -> both valves shut, muscle sealed
//
// The HOLD band (gap between OPEN_LEVEL and CLOSE_LEVEL) is a deadband. Because the
// muscle moves the same finger the sensor measures, this deadband is what keeps the
// loop stable — without it you get chatter/oscillation at the threshold.
//
// DIRECTION ASSUMPTION: inflating the muscle ASSISTS closing (helps make a fist).
// If your muscle is arranged to EXTEND the finger instead, swap the roles of
// channelA / channelB in the loop (or invert the pct logic).
//
// PAIRING: active flex sensor i drives active valve pair i. So enable the SAME
// number of flex sensors and valve pairs, in the order you want them paired.

#define BUFFER_SIZE 10

struct sensor {
  int pin;
  bool active;
};

struct flex_values {
  int buffer[BUFFER_SIZE];
  int index;
  int count;
  int difference;   // kept for optional feedforward; unused by default
};

struct valve {
  int pinA;      // inlet valve
  int pinB;      // exhaust valve
  int channelA;  // inlet pwm ledc channel
  int channelB;  // exhaust pwm ledc channel
  bool active;
};

// Prefer ADC1 pins (GPIO 32-39) for analog reads — ADC2 pins are noisier and
// unusable while WiFi is on. 39/35/33 are ADC1; 26/14 are ADC2.
sensor Flex[] = {
  {39, true},
  {35, true},
  {33, true},
  {26, true},
  {14, false}
};

sensor FSR[] = {
  {36, false},
  {34, false},
  {32, false},
  {25, false},
  {27, false}
};

// Enable one valve pair per active flex sensor, in matching order.
valve Valves[] = {
  {2,  15, 0, 1, true},
  {16,  4, 2, 3, true},
  {5,  17, 4, 5, true},
  {19, 18, 6, 7, true},
  {3,  21, 8, 9, false},
};

sensor active_flex[5];
int activeFlex = 0;

sensor active_fsr[5];
int activeFSR = 0;

valve active_valves[5];
int activeValves = 0;

flex_values Flex_Values[5];

// Per-finger calibration: raw analogRead at fully open / fully closed.
int flexOpenRaw[5];
int flexCloseRaw[5];

// ---- Tuning (in normalized 0-100% flex, so it's the same across all fingers) ----
const int CLOSE_LEVEL = 60;   // above this % -> inflate. Raise to require firmer grip intent.
const int OPEN_LEVEL  = 35;   // below this % -> deflate. The gap (35..60) is the HOLD deadband.
const int MIN_DUTY    = 60;   // smallest PWM that actually cracks the valve (find by experiment)
const int MAX_DUTY    = 255;

// ---------------------------------------------------------------------------

void push_value(flex_values &f, int new_value) {
  f.buffer[f.index] = new_value;
  f.index = (f.index + 1) % BUFFER_SIZE;
  if (f.count < BUFFER_SIZE) f.count++;
}

int get_average(flex_values &f) {
  if (f.count == 0) return 0;
  long sum = 0;
  for (int i = 0; i < f.count; i++) sum += f.buffer[i];
  return sum / f.count;
}

// Blocking average used only during calibration.
int readAveraged(int pin, int samples = 64) {
  long s = 0;
  for (int i = 0; i < samples; i++) { s += analogRead(pin); delay(2); }
  return s / samples;
}

// Records each active finger's open and closed raw readings.
void calibrateFlex() {
  Serial.println();
  Serial.println("=== Flex calibration ===");
  Serial.println("Relax / open your hand. Sampling OPEN in 3s...");
  delay(3000);
  for (int i = 0; i < activeFlex; i++) {
    flexOpenRaw[i] = readAveraged(active_flex[i].pin);
    Serial.print("  finger "); Serial.print(i);
    Serial.print(" open="); Serial.println(flexOpenRaw[i]);
  }
  Serial.println("Make a FULL FIST. Sampling CLOSED in 3s...");
  delay(3000);
  for (int i = 0; i < activeFlex; i++) {
    flexCloseRaw[i] = readAveraged(active_flex[i].pin);
    Serial.print("  finger "); Serial.print(i);
    Serial.print(" closed="); Serial.println(flexCloseRaw[i]);
  }
  Serial.println("Calibration done.");
  Serial.println();
}

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 5; i++)
    if (Flex[i].active) { active_flex[activeFlex++] = Flex[i]; Serial.println(Flex[i].pin); }
  for (int i = 0; i < 5; i++)
    if (FSR[i].active)  { active_fsr[activeFSR++] = FSR[i]; }
  for (int i = 0; i < 5; i++)
    if (Valves[i].active) { active_valves[activeValves++] = Valves[i]; Serial.println(Valves[i].pinA); }

  if (activeFlex != activeValves)
    Serial.println("WARNING: active flex count != active valve count. They are paired by index!");

  for (int i = 0; i < activeFlex; i++)  pinMode(active_flex[i].pin, INPUT);
  for (int i = 0; i < activeFSR; i++)   pinMode(active_fsr[i].pin, INPUT);

  for (int i = 0; i < activeValves; i++) {
    pinMode(active_valves[i].pinA, OUTPUT);
    pinMode(active_valves[i].pinB, OUTPUT);
    ledcSetup(active_valves[i].channelA, 50, 8);   // see note: verify 50 Hz suits your valve
    ledcAttachPin(active_valves[i].pinA, active_valves[i].channelA);
    ledcSetup(active_valves[i].channelB, 50, 8);
    ledcAttachPin(active_valves[i].pinB, active_valves[i].channelB);
  }

  calibrateFlex();
}

void loop() {
  for (int i = 0; i < activeFlex; i++) {
    push_value(Flex_Values[i], analogRead(active_flex[i].pin));
    int raw = get_average(Flex_Values[i]);   // noise-smoothed reading (low-pass)

    // Normalize to 0 (open) .. 100 (closed). Handles reversed sensors automatically
    // because span can be negative; constrain clamps the ends.
    int span = flexCloseRaw[i] - flexOpenRaw[i];
    if (span == 0) span = 1;                 // guard against a dead/uncalibrated sensor
    int pct = constrain((long)(raw - flexOpenRaw[i]) * 100 / span, 0, 100);

    const char* state;
    if (pct >= CLOSE_LEVEL) {                          // user is closing -> inflate
      int duty = constrain(map(pct, CLOSE_LEVEL, 100, MIN_DUTY, MAX_DUTY), 0, 255);
      ledcWrite(active_valves[i].channelA, duty);      // inlet open
      ledcWrite(active_valves[i].channelB, 0);         // exhaust shut
      state = "INFLATE";
    } else if (pct <= OPEN_LEVEL) {                    // user is opening -> deflate
      ledcWrite(active_valves[i].channelA, 0);         // inlet shut
      ledcWrite(active_valves[i].channelB, MAX_DUTY);  // exhaust open (full vent; make proportional for gentler release)
      state = "DEFLATE";
    } else {                                           // deadband -> hold
      ledcWrite(active_valves[i].channelA, 0);
      ledcWrite(active_valves[i].channelB, 0);         // both shut: muscle sealed at current pressure
      state = "HOLD";
    }

    Serial.print("F"); Serial.print(i);
    Serial.print(" raw="); Serial.print(raw);
    Serial.print(" pct="); Serial.print(pct);
    Serial.print(" ["); Serial.print(state); Serial.print("]  ");
  }
  Serial.println();

  delay(15);   // ~65 Hz. Keep small so the smoothing buffer stays fresh — do NOT use 200.
}
