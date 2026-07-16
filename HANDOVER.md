# HANDOVER — SUMOBOTS sim + firmware (session of 2026-07-16)

For the next agent/chat picking this project up. Read this, then **PLAN.md → "Invariants &
gotchas"** before touching any code. Repo: `C:\Users\micha\Desktop\Project\bots\sumo-sim`,
remote `https://github.com/c888michael-sys/sumobot`, branch `main`.

## What this project is

RAMSoc **SUMOBOTS** (autonomous robot sumo, 120 cm ring, best-of-3, 5 s start freeze). The user
is building toward a real competition robot. Strategy pipeline: prototype in the one-file
simulator (`index.html`) → score in the built-in ⚔ headless tournament harness → port the
winner to Arduino firmware (`firmware/`). PLAN.md is the authoritative roadmap; RULES.md the
condensed rulebook (no equipment/kit list in it — only constraints; the official RAMSoc
rulebook may have one).

**The user's own strategy — the current focus — is the "Prototype side wedge":** an asymmetric
front wedge, LOW LEFT corner (`wedgeL≈0.05`) / higher right (`wedgeR≈0.70`), paired with
`bots/wedge-lever.js`: arc in slightly right so the low left corner slips under → lever left to
swing onto the opponent's flank → push out through the exposed side.

## State of the repo

Phases 1–3 of PLAN.md are ✅ done (simulator, headless harness with variance injection +
side-balancing, five-bot teaching zoo). Also done from the feature list: split wedge
(`wedgeL`/`wedgeR` + `wedgeAt()`), ride-up consequence (overlap bias, inelastic impulse,
traction strip + velocity damp), `fxLog()` effect logger, chassis presets, keyboard mode,
match recorder — check PLAN.md status notes per feature.

### Done this session (committed + pushed, `5f416c7`)

**Wedge turning lever** — the last missing wedge feature. In `resolve()` in `index.html`
(anchor on the `// ---- wedge lever` comment, ~line 465): when a bot is under the other
(`liftA`/`liftB > 0`), the under-bot's forward drive is applied as an impulse **at the contact
point** (`applyImpulse(B, J, rb)`), so the off-centre `cross(r, J)` torque rotates the lifted
opponent. Emits `↻ A levers B around (turning)` via `fxLog`.

- **`TURN = 0.005`, NOT the 0.6 in PLAN.md's snippet.** The impulse accumulates every 120 Hz
  tick; 0.6 would spin bots at hundreds of rad/s. 0.005 gives ~64 deg/s equilibrium rotation
  for a one-side-low engagement — visible, not chaotic.
- Symmetric engagements can't spin: equal wedge heights ⇒ `liftA = liftB = 0` ⇒ block never fires.
- Verified live in the browser (A with 0.05/0.70 + wedge-lever bot beat symmetric B 2–0,
  lever lines in console) and sanity-checked in the harness.

### Done this session (⚠ UNCOMMITTED — user hasn't confirmed the commit yet)

**Phase 5 firmware port**: `firmware/sumobot.ino` + `firmware/README.md`. Arduino Nano port of
`wedge-lever.js` with the same phase machine (approach → lever → push), edge-guard preamble,
5 s freeze (sensors live), search-toward-last-bearing. **User's confirmed sensor loadout:
3× HC-SR04 ultrasonic (−30/0/+30°, round-robin pinged one per tick, single-pin wiring) +
1× Sharp IR ranger (forward)**, plus 4× corner reflectance for edges and a TB6612/L298N
driver. ⚠ Compliance question flagged: do the reflectance edge sensors count against the
4-IR cap? **Compile-untested** (no toolchain on this machine). Deliberate hardware deviations
from the sim, all documented in `firmware/README.md`:

1. Time-based thresholds (`ENGAGE_MS=150`) instead of tick counts.
2. `LEVER_EXIT_DEG=25` not the sim's 45° — fused bearing is a weighted average of mount angles
   so it can never exceed ±30°; 45° would never fire. `LEVER_MAX_MS=600` timeout as fallback.
3. IR must be recessed ~10 cm behind the wedge tip (Sharp folds back <10 cm = garbage at
   contact); optional wedge bump switch (`USE_BUMP`) is the robust engagement signal.
4. 120 ms edge-evasive latch (real reflectance sensors chatter at the line).
5. `EDGE_WHITE_IS_LOW` polarity flag — inverted edge sense = instant self-out; calibrate first.

## Project docs (all in repo root unless noted)

| Doc | What it holds |
|---|---|
| `PLAN.md` | Authoritative roadmap: phases, feature specs, **Invariants & gotchas**, Bot API contract |
| `RULES.md` | Condensed rulebook (below); the official RAMSoc doc is authoritative |
| `README.md` | Sim usage / feature list |
| `bots/README.md` | Measured meta findings (why flanker/juker lose to rammer at equal chassis) |
| `firmware/README.md` | Wiring, calibration order, sim↔firmware differences |

## Rulebook essentials (condensed from RULES.md — note: NO equipment/kit list exists in it)

- **Objective:** push the opponent out of a **120 cm ring** (115 cm inner, 2.5 cm white border,
  black-painted MDF). Fully **autonomous** — remote control banned.
- **Match:** best of 3; rounds up to 2 min; **5 s freeze** after the start call before moving.
  One 2-min timeout per match.
- **Win a round:** any part of the opponent out of the ring / opponent falls out / opponent
  stops moving ≥ 5 s. Falling over while still in the ring is NOT a loss.
- **Rematches:** entangled or orbiting with no progress 5 s; both stopped 5 s. No winner at
  time → extra round; persistent ties → judge decision.
- **Streams:** **Standard** (UNSW only): ≤200×200 mm, ≤1 kg, 1–5 A fuse, ≤4 IR + ≤4 ultrasonic,
  max 2 driven wheels, no drivetrain mods. **Open**: ≤250×250 mm, ≤1.5 kg, $200 budget ($100 =
  RAMSoc base kit), 1–10 A fuse, unrestricted drivetrain (still max 2 driven wheels). Height
  unlimited both streams; PLA/PETG 3D printing is budget-free.
- **Starts:** 4 starting positions (Figure 2), chosen before the match, opposing sides, can't
  change until next round. "Forward" = whatever the wedge/main sensor faces.
- **Banned:** RC · IR jamming · weapons · liquids/powders/gases · throwing · traction adhesives
  · magnets/vacuum adhesion · sharp edges (radius must exceed 0.005″) · intentional damage.
- **Violations:** major (non-compliant robot, insulting conduct) = lose the match; two minors
  (early start, excessive delay, entering the ring, unsportsmanlike) = lose the round.
- **Boss battles:** optional matches vs a RAMSoc bossbot that may break one spec.

## Open items / next steps

1. **Commit the firmware** if the user confirms (they were asked; no answer yet).
2. **Get the RAMSoc base-kit BOM** from the user, then adapt firmware pins/sensor curves/driver.
   First hardware datapoint (2026-07-16): the user's rangers read **~5 cm short** of true
   distance — corrected via `US_OFFSET_CM`/`IR_OFFSET_CM = 5` in `sumobot.ino`; re-verify on
   the final mount (if measured from the wedge tip, it's recess geometry → set offsets to 0).
3. Ringside calibration is spec'd in `firmware/README.md` (order matters: edge polarity → IR
   curve → MIN_DUTY → trim → arc ratios).
4. Optional sim work: Phase 4 sensor noise; possibly retune `TURN` after more harness runs.
5. Harness datapoint from this session: asym side-wedge + wedge-lever bot vs symmetric default
   ≈ 56% / 42% / 2% draws over 50. Symmetric-vs-symmetric runs skew if you forget Bot A still
   has wedge-lever *code* loaded — equalize code AND chassis for a true 50/50 baseline.

## Critical gotchas (bite hard — full list in PLAN.md)

- **One file.** All sim code stays in `index.html`, one `<script>`, no build, no deps.
- **Engine is deterministic** — never add randomness to physics; tournament variance comes only
  from `randomizeStart()`/JITTER in the harness.
- **Headless driving:** set `running=false` first (else rAF double-steps), then `startMatch()`
  and loop `step()` with an iteration cap. `runMatches(n)` does all this; use it.
- Coordinates: y is DOWN; bearing + = right. `memory` resets every round.
- DOM handles for scripting the page: textareas are `#codeA`/`#codeB` (no data attribute);
  starts `select[data-start="A"|"B"]`; cfg fields `input[data-k][data-bot="A"|"B"]`; call
  `save()` after mutating cfg. Serve over http via `.claude/launch.json` config `sumo-sim`
  (python http.server; the `bots/` fetch-loader and presets need http, not `file://`).
- Verify in-browser via console/eval, not screenshot-idle tools (continuous rAF loop). fxLog
  output goes to the **in-page** console panel, not the browser console.
- Keep `index.html` runnable at every commit; commit messages end with a Claude co-author line
  (see `git log`).
