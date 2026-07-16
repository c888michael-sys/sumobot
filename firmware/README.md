# Firmware — "Prototype side wedge" (Phase 5 port)

`sumobot.ino` is the hardware port of [bots/wedge-lever.js](../bots/wedge-lever.js) + the
asymmetric-wedge chassis (the sim's "Prototype side wedge" preset: `wedgeL≈0.05`, `wedgeR≈0.30`).
The strategy is unchanged: **arc in slightly right so the low LEFT wedge corner makes first
contact and slips under → lever left to swing onto their flank → drive them out.**

## Build requirements (the chassis IS half the strategy)

- Front wedge with the **left corner as low as manufacturable** (floor-scraping), right side
  higher. Edge radius must exceed 0.005″ (rules — no sharp edges).
- **Sensor loadout: 3× HC-SR04 ultrasonic (−30°/0°/+30°) + 1× Sharp IR ranger (forward).**
  Ultrasonics work down to ~2 cm, so they cover contact range natively. Mount the one Sharp
  IR **recessed ~10 cm behind the wedge tip** (its output folds back below 10 cm).
- Ultrasonics are pinged **round-robin, one per 20 ms tick** (full sweep every 60 ms) — three
  blocking pings per tick would blow the loop budget, and simultaneous pings cross-talk.
- Optional: a **limit switch on the wedge face** (`USE_BUMP 1`) — a more reliable "we're
  engaged" signal than any ranging.
- ⚠ **Compliance:** confirm with RAMSoc whether the downward reflectance edge sensors count
  against the Standard stream's "up to 4 IR sensors" cap — 4 edge + 1 IR ranger would be 5
  if so (drop to 3 edge sensors or the IR ranger if it does).
- Stream limits: Standard ≤ 200×200 mm, ≤ 1 kg, 1–5 A fuse fitted, max 2 driven wheels.

## Default pin map (Arduino Nano)

| Function | Pins |
|---|---|
| Left motor (TB6612/L298N) | PWM D5, IN D4/D7 |
| Right motor | PWM D6, IN D8/D9 |
| Driver STBY (TB6612) | D10 (set `-1` and tie STBY high if you need the pin) |
| Start button (to GND) | D2 |
| Edge reflectance FL/FR/BL/BR | A0/A1/A2/A3 |
| HC-SR04 −30°/0°/+30° | D3 / D11 / D12 — **single-pin mode** (TRIG+ECHO tied together) |
| Sharp IR ranger (0°) | A4 |
| Bump switch (optional) | D13 (onboard LED can fight the pullup on some boards) |

Single-pin HC-SR04 mode ties each module's TRIG and ECHO to one Arduino pin; the sketch
mode-switches around each ping. Most modules/clones handle this fine — if yours misbehaves,
give each module its own ECHO pin in `US_ECHO[]` (free pins by tying STBY high).

## Calibration checklist (in order, before the first bout)

1. **Edge polarity** — set `DEBUG 1`, hold the bot over the black ring, then the white border.
   QTR-style analog boards read **LOW over white**; if yours is inverted, flip
   `EDGE_WHITE_IS_LOW`. Then pick `EDGE_THRESH` midway between the two readings.
   *An inverted edge sense is an instant self-out — verify this first.*
2. **Ranging** — check `d=` and the per-sensor `us=` values in the DEBUG output against a tape
   measure at 20/40/60 cm. A constant under-read is corrected by `US_OFFSET_CM` /
   `IR_OFFSET_CM` (both currently **+5 cm**, from bench measurement — re-verify each on the
   final mount; if you measured from the wedge tip rather than the sensor face, the offset is
   recess geometry and belongs at 0). Confirm contact with another robot reads < `CLOSE_CM`
   (15). Adjust the `irReadCm()` curve if your IR isn't a GP2Y0A21.
3. **`MIN_DUTY`** — lowest PWM that actually turns the wheels under the bot's own weight.
4. **`TRIM_L/R`** — run `drive(1,1)` across the ring; trim the faster side down until it tracks
   straight.
5. **Drive shape, ringside** — `APPROACH_ARC` (slight right arc; too low and you spiral, too
   high and the low corner doesn't lead), `LEVER_INSIDE` (lever aggressiveness), `SEARCH_SPIN`
   (fast enough to sweep, slow enough that IR gets valid reads).

## Sim ↔ firmware differences (deliberate)

- **Time-based thresholds, not ticks** — the sim counts 8 ticks @120 Hz for engagement;
  firmware uses `ENGAGE_MS` (150 ms) so loop-rate jitter doesn't change behaviour.
- **`LEVER_EXIT_DEG` = 25, not 45** — the fused bearing is a weighted average of the mount
  angles, so with ±30° mounts it can never exceed ±30°. The sim's 45° would never fire.
  `LEVER_MAX_MS` (600 ms) is the fallback: if the swing can't be confirmed, commit to the push.
- **Edge evasive latch** (`EDGE_LATCH_MS` 120 ms) — real reflectance sensors chatter at the
  line boundary; the latch holds each evasive long enough to actually clear it.

## Rules reminders

5 s freeze is enforced after the button press (sensors live, motors dead — it plans its search
direction during the freeze, same as the sim bot). Search always spins, so the bot can't lose
to the 5 s stationary rule. No remote control: the button is the only input.
