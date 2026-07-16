/*
 * sumobot.ino — "Prototype side wedge" strategy (Phase 5 port of bots/wedge-lever.js)
 *
 * BUILD REQUIREMENT: this code assumes the asymmetric wedge chassis — the front wedge's
 * LEFT corner is the low one (near floor-scraping), the right side higher. The approach
 * arcs slightly right so that low left corner leads into contact and slips under, then
 * the bot levers left to swing onto the opponent's flank and drives them out.
 *
 * Sensor loadout: 3× HC-SR04 ultrasonic (−30°/0°/+30°) + 1× Sharp IR ranger (forward).
 * Ultrasonics are pinged ROUND-ROBIN, one per control tick — three blocking pings per
 * tick would blow the 20 ms loop budget, and simultaneous pings cross-talk. Readings
 * are cached and fused; a reading older than US_STALE_MS is ignored.
 *
 * Sim → hardware mapping (PLAN.md Phase 5):
 *   enemy / sensors   → 3 US + 1 IR fused into {detected, distance, bearing} below
 *   edge.fl/fr/bl/br  → downward reflectance sensors at the corners (white-line detect)
 *   setMotors(l, r)   → drive(l, r): PWM to the two motor drivers, -1..1 per wheel
 *   memory            → static globals (phase, lastBearing, timers)
 *   control tick      → loop() at a fixed LOOP_MS rate
 *
 * Rules honoured: 5 s start freeze after the button press (sensors live, motors dead);
 * search always spins so the bot never sits still (> 5 s stationary = loss); max 2
 * driven wheels; the wedge is passive. Fit your stream's fuse (1–5 A Standard).
 * ⚠ COMPLIANCE CHECK: confirm with RAMSoc whether the downward reflectance edge sensors
 * count against the "up to 4 IR sensors" cap — 4 edge + 1 IR ranger would be 5 if so.
 */

#include <Arduino.h>
#include <math.h>

/* ================= PINS — EDIT to match your wiring ================= */
// Motor driver: TB6612FNG-style (PWM + two direction pins per motor).
// For an L298N wire ENA/ENB to the PWM pins and IN1..IN4 to the direction pins.
const uint8_t PIN_L_PWM = 5,  PIN_L_IN1 = 4,  PIN_L_IN2 = 7;   // left wheel
const uint8_t PIN_R_PWM = 6,  PIN_R_IN1 = 8,  PIN_R_IN2 = 9;   // right wheel
const int8_t  PIN_STBY  = 10;                                  // TB6612 STBY; set -1 if unused

const uint8_t PIN_START_BTN = 2;   // momentary button to GND (INPUT_PULLUP); press starts the 5 s freeze

// Downward corner reflectance sensors (analog). f=front, b=back, l=left, r=right.
const uint8_t PIN_EDGE_FL = A0, PIN_EDGE_FR = A1, PIN_EDGE_BL = A2, PIN_EDGE_BR = A3;

// 3× HC-SR04 ultrasonic at mount angles. Default wiring is SINGLE-PIN mode (TRIG and
// ECHO tied together on one pin — the code mode-switches around each ping), which fits
// the Nano's pin budget. If your modules misbehave single-pin, give each its own ECHO
// pin and update US_ECHO (you'll need to free pins — e.g. tie STBY high and set -1).
const uint8_t N_US = 3;
const uint8_t US_TRIG[N_US]  = { 3, 11, 12 };
const uint8_t US_ECHO[N_US]  = { 3, 11, 12 };   // == TRIG → single-pin mode
const int8_t  US_ANGLE[N_US] = { -30, 0, 30 };  // deg, + = right (same sign convention as the sim)

// 1× Sharp IR ranger (GP2Y0A21YK0F, valid 10–80 cm), facing forward. Mount it recessed
// ~10 cm behind the wedge tip — Sharp output folds back below 10 cm. (The ultrasonics
// cover contact range natively, so the IR is a second opinion, not the close sensor.)
const uint8_t PIN_IR   = A4;
const int8_t  IR_ANGLE = 0;

#define USE_BUMP 0           // 1 = limit switch on the wedge face — the robust "we're engaged" signal
#if USE_BUMP
const uint8_t PIN_BUMP = 13; // switch to GND, INPUT_PULLUP (note: D13's onboard LED can fight the
                             // pullup on some boards — move it if reads are flaky)
#endif

#define DEBUG 0              // 1 = phase/sensor prints on Serial @115200 (adds loop time — off for bouts)

/* ================= TUNING — calibrate ringside, see firmware/README.md ================= */
const uint16_t LOOP_MS   = 20;      // 50 Hz control tick (sim runs 120 Hz; thresholds below are time-based)
const uint16_t FREEZE_MS = 5000;    // rules: 5 s start delay

// Edge sensing. QTR-style analog boards read LOW over the white border (more reflection);
// some digital boards are inverted. VERIFY with DEBUG before the first bout — an inverted
// edge sense is an instant self-out.
#define EDGE_WHITE_IS_LOW 1
const int EDGE_THRESH = 500;        // analog split point between black ring and white border. CALIBRATE

// Enemy sensing. Offsets correct the measured ~5 cm under-read so everything downstream
// (CLOSE_CM, LOST_CM, bearing fusion) works in TRUE centimetres. Re-verify each sensor
// type on the final mount — if the "under-read" was measured from the wedge tip rather
// than the sensor face, it's recess geometry, not sensor error, and belongs at 0.
const float    US_OFFSET_CM = 5.0f; // HC-SR04 correction
const float    IR_OFFSET_CM = 5.0f; // Sharp correction
const float    SENSE_MAX_CM = 80;   // detection range (sim SENSOR_RANGE)
const uint16_t US_STALE_MS  = 150;  // ignore cached ultrasonic readings older than this
const float    CLOSE_CM     = 15;   // "contact range" (sim: 14)
const float    LOST_CM      = 22;   // push phase: disengage beyond this (sim: 22)

// The lever maneuver.
const uint16_t ENGAGE_MS      = 150;  // sustained-close time = "low corner is under" (sim: 8 ticks ≈ 67 ms)
const float    LEVER_EXIT_DEG = 25;   // bearing swing = "flank exposed". Sim used 45°, but the fused
                                      // bearing can never exceed the outermost mount angle (±30° here),
                                      // so exit just inside it — or on the timeout below.
const uint16_t LEVER_MAX_MS   = 600;  // FOV fallback: if the swing can't be confirmed, commit to the push
const uint16_t EDGE_LATCH_MS  = 120;  // hold an edge evasive at least this long (debounces sensor chatter)

// Drive shape (duty ratios; real drivetrains differ from the sim — tune these ringside).
const float APPROACH_ARC = 0.78;    // right-wheel duty on approach: slight right arc → low LEFT corner leads
const float LEVER_INSIDE = 0.55;    // left-wheel duty while levering left (right wheel at 1.0)
const float PUSH_ARC     = 0.2;     // inside-wheel duty when steering during the push
const float STEER_DEG    = 8;       // push-phase steering deadband
const float SEARCH_SPIN  = 0.55;    // search spin duty

// Motor linearisation.
const uint8_t MIN_DUTY = 40;        // PWM below this doesn't turn the wheels. CALIBRATE
const float   TRIM_L = 1.00, TRIM_R = 1.00;  // balance so drive(1,1) tracks straight. CALIBRATE

/* ================= types + state ("memory" in the sim) ================= */
struct Edge  { bool fl, fr, bl, br; };
struct Enemy { bool detected; float distance; float bearing; };
enum Phase : uint8_t { APPROACH, LEVER, PUSH };

Phase    phase        = APPROACH;
float    lastBearing  = 0;      // sim: memory.lastBearing — which side to search toward
uint32_t closeSinceMs = 0;      // sim: memory.eng — 0 = not currently close
uint32_t leverStartMs = 0;
uint32_t freezeEndMs  = 0;
uint8_t  edgeLatch    = 0;      // 0 none · 1 both-front · 2 FL · 3 FR · 4 rear
uint32_t edgeLatchEnd = 0;
uint32_t lastTickMs   = 0;

// Ultrasonic round-robin cache.
float    usDist[N_US] = { NAN, NAN, NAN };
uint32_t usSeen[N_US] = { 0, 0, 0 };
uint8_t  usNext       = 0;

/* ================= motors ================= */
void motorWrite(uint8_t pwmPin, uint8_t in1, uint8_t in2, float cmd, float trim) {
  cmd = constrain(cmd * trim, -1.0f, 1.0f);
  digitalWrite(in1, cmd >= 0 ? HIGH : LOW);
  digitalWrite(in2, cmd >= 0 ? LOW  : HIGH);
  float mag = fabsf(cmd);
  analogWrite(pwmPin, mag < 0.02f ? 0 : (uint8_t)(MIN_DUTY + mag * (255 - MIN_DUTY)));
}

// setMotors(l, r) from the sim: each wheel -1..1.
void drive(float l, float r) {
  motorWrite(PIN_L_PWM, PIN_L_IN1, PIN_L_IN2, l, TRIM_L);
  motorWrite(PIN_R_PWM, PIN_R_IN1, PIN_R_IN2, r, TRIM_R);
}

/* ================= sensors ================= */
bool overWhite(uint8_t pin) {
  int raw = analogRead(pin);
#if EDGE_WHITE_IS_LOW
  return raw < EDGE_THRESH;
#else
  return raw > EDGE_THRESH;
#endif
}

Edge readEdges() {
  Edge e;
  e.fl = overWhite(PIN_EDGE_FL); e.fr = overWhite(PIN_EDGE_FR);
  e.bl = overWhite(PIN_EDGE_BL); e.br = overWhite(PIN_EDGE_BR);
  return e;
}

// One HC-SR04 ping. Handles single-pin mode (TRIG == ECHO) by mode-switching.
float usPingCm(uint8_t i) {
  uint8_t t = US_TRIG[i], e = US_ECHO[i];
  if (t == e) pinMode(t, OUTPUT);
  digitalWrite(t, LOW);  delayMicroseconds(2);
  digitalWrite(t, HIGH); delayMicroseconds(10);
  digitalWrite(t, LOW);
  if (t == e) pinMode(e, INPUT);
  uint32_t us = pulseIn(e, HIGH, 5000);      // ~86 cm cap → ≤5 ms block, keeps the 50 Hz tick
  if (!us) return NAN;
  float d = us / 58.0f + US_OFFSET_CM;
  return d > SENSE_MAX_CM ? NAN : d;
}

// GP2Y0A21YK0F voltage→cm. Swap the curve if you mount a different ranger. CALIBRATE.
float irReadCm(uint8_t pin) {
  float v = analogRead(pin) * (5.0f / 1023.0f);
  if (v < 0.45f) return NAN;                 // beyond range — nothing there
  float d = 27.86f * powf(v, -1.15f) + IR_OFFSET_CM;
  if (d < 4 || d > SENSE_MAX_CM) return NAN;
  return d;
}

// Fold one valid reading into the fused enemy estimate.
void fold(Enemy &en, float &wSum, float &aSum, float d, int8_t angle) {
  en.detected = true;
  if (d < en.distance) en.distance = d;
  float w = SENSE_MAX_CM - d;                // nearer readings dominate the bearing
  wSum += w; aSum += w * angle;
}

// Fuse like the sim's `enemy`: detected / nearest distance / proximity-weighted bearing.
// Pings ONE ultrasonic per call (round-robin) — full sweep every 3 ticks (60 ms).
Enemy senseEnemy() {
  uint32_t now = millis();
  usDist[usNext] = usPingCm(usNext);
  usSeen[usNext] = now;
  usNext = (usNext + 1) % N_US;

  Enemy en = { false, 1e9f, 0 };
  float wSum = 0, aSum = 0;
  for (uint8_t i = 0; i < N_US; i++) {
    if (isnan(usDist[i]) || now - usSeen[i] > US_STALE_MS) continue;
    fold(en, wSum, aSum, usDist[i], US_ANGLE[i]);
  }
  float ir = irReadCm(PIN_IR);
  if (!isnan(ir)) fold(en, wSum, aSum, ir, IR_ANGLE);
  if (en.detected && wSum > 0) en.bearing = aSum / wSum;
  return en;
}

bool bumpPressed() {
#if USE_BUMP
  return digitalRead(PIN_BUMP) == LOW;
#else
  return false;
#endif
}

/* ================= setup: arm → button → 5 s freeze ================= */
void setup() {
  pinMode(PIN_L_PWM, OUTPUT); pinMode(PIN_L_IN1, OUTPUT); pinMode(PIN_L_IN2, OUTPUT);
  pinMode(PIN_R_PWM, OUTPUT); pinMode(PIN_R_IN1, OUTPUT); pinMode(PIN_R_IN2, OUTPUT);
  if (PIN_STBY >= 0) { pinMode(PIN_STBY, OUTPUT); digitalWrite(PIN_STBY, HIGH); }
  pinMode(PIN_START_BTN, INPUT_PULLUP);
  for (uint8_t i = 0; i < N_US; i++) {
    if (US_TRIG[i] != US_ECHO[i]) { pinMode(US_TRIG[i], OUTPUT); pinMode(US_ECHO[i], INPUT); }
    // single-pin sensors are mode-switched per ping in usPingCm()
  }
#if USE_BUMP
  pinMode(PIN_BUMP, INPUT_PULLUP);
#endif
#if DEBUG
  Serial.begin(115200);
  Serial.println(F("side-wedge: armed, waiting for start button"));
#endif
  drive(0, 0);
  while (digitalRead(PIN_START_BTN) == HIGH) {}   // wait for the press
  delay(30);                                      // debounce
  freezeEndMs = millis() + FREEZE_MS;
}

/* ================= control tick — mirrors bots/wedge-lever.js ================= */
void loop() {
  uint32_t now = millis();
  if (now - lastTickMs < LOOP_MS) return;
  lastTickMs = now;

  Edge  e  = readEdges();
  Enemy en = senseEnemy();

#if DEBUG
  static uint32_t dbgMs = 0;
  if (now - dbgMs > 250) {
    dbgMs = now;
    Serial.print(F("ph=")); Serial.print(phase);
    Serial.print(F(" det=")); Serial.print(en.detected);
    Serial.print(F(" d=")); Serial.print(en.distance);
    Serial.print(F(" brg=")); Serial.print(en.bearing);
    Serial.print(F(" us=")); Serial.print(usDist[0]); Serial.print('/');
    Serial.print(usDist[1]); Serial.print('/'); Serial.print(usDist[2]);
    Serial.print(F(" edge=")); Serial.print(e.fl); Serial.print(e.fr);
    Serial.print(e.bl); Serial.println(e.br);
  }
#endif

  // --- 5 s start freeze: motors dead, sensors live — plan into memory (sim: frozen) ---
  if (now < freezeEndMs) {
    if (en.detected) lastBearing = en.bearing;
    drive(0, 0);
    return;
  }

  // --- SELF-PRESERVATION first — overrides every phase (sim: edge guard preamble) ---
  uint8_t evade = 0;
  if      (e.fl && e.fr) evade = 1;
  else if (e.fl)         evade = 2;
  else if (e.fr)         evade = 3;
  else if (e.bl || e.br) evade = 4;
  if (evade) { edgeLatch = evade; edgeLatchEnd = now + EDGE_LATCH_MS; }
  else if ((int32_t)(now - edgeLatchEnd) >= 0) edgeLatch = 0;

  if (edgeLatch) {
    switch (edgeLatch) {
      case 1: drive(-1, -1);       break;   // both front corners on the line → straight back
      case 2: drive(0.6f, -1);     break;   // front-left → back off swinging right
      case 3: drive(-1, 0.6f);     break;   // front-right → back off swinging left
      case 4: drive(1, 1);         break;   // rear corner → drive forward into the ring
    }
    if (edgeLatch != 4) { phase = APPROACH; closeSinceMs = 0; }
    return;
  }

  // --- ENGAGEMENT: approach → lever → push (the side-wedge maneuver) ---
  if (en.detected) {
    lastBearing = en.bearing;
    bool close = en.distance < CLOSE_CM || bumpPressed();

    switch (phase) {
      case APPROACH:
        // Arc in with a slight right bias so the LOW LEFT wedge corner leads into contact.
        drive(1, APPROACH_ARC);
        if (close) { if (!closeSinceMs) closeSinceMs = now; }
        else closeSinceMs = 0;
        if (closeSinceMs && now - closeSinceMs >= ENGAGE_MS) {   // proxy for "low corner is under"
          phase = LEVER; leverStartMs = now;
        }
        break;

      case LEVER:
        // Turn left while pushing (right wheel faster) → lever them around, swing onto the flank.
        drive(LEVER_INSIDE, 1);
        if (fabsf(en.bearing) > LEVER_EXIT_DEG ||
            now - leverStartMs >= LEVER_MAX_MS) phase = PUSH;    // flank exposed (or FOV can't confirm — commit)
        if (!close) { phase = APPROACH; closeSinceMs = 0; }      // slipped off — re-approach
        break;

      case PUSH:
        // Drive through their exposed side.
        if      (en.bearing >  STEER_DEG) drive(1, PUSH_ARC);
        else if (en.bearing < -STEER_DEG) drive(PUSH_ARC, 1);
        else                              drive(1, 1);
        if (en.distance > LOST_CM) { phase = APPROACH; closeSinceMs = 0; }
        break;
    }
    return;
  }

  // --- SEARCH: spin toward the last known side. Always moving — a bot stationary > 5 s loses. ---
  phase = APPROACH; closeSinceMs = 0;
  float dir = lastBearing >= 0 ? 1 : -1;
  drive(SEARCH_SPIN * dir, -SEARCH_SPIN * dir);
}
