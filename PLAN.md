# Build plan — SUMOBOTS

This is the roadmap for finishing the project. **Phase 1 (the simulator) is done.**
The remaining phases are written so another agent (e.g. Claude Sonnet) or a human can pick
them up cold. Build everything against the simulator first, then port to hardware.

---

## Context

RAMSoc's **SUMOBOTS** is autonomous robot sumo: push the opponent out of a 120 cm ring.
No remote control. 5 s start freeze. Best-of-3, 2 min rounds. A robot that stops moving for
> 5 s loses. Two streams:

- **Standard** — UNSW only. Max 200 mm × 200 mm, 1 kg. Up to 4 IR + 4 ultrasonic sensors.
  Max 2 driven wheels, no drivetrain mods. 1–5 A fuse.
- **Open** — anyone. Max 250 mm × 250 mm, 1.5 kg. $200 budget ($100 on the RAMSoc base kit).
  Unrestricted drivetrain (still max 2 driven wheels). 1–10 A fuse.

Banned: remote control, jamming (e.g. IR-saturating LEDs), weapons, liquids/powders/gases,
throwing mechanisms, traction adhesives, magnets/vacuum for adhesion, sharp edges
(radius must exceed 0.005″). Full summary in [RULES.md](RULES.md).

## The competitive meta (design principles the bots should embody)

1. **Never self-out.** Edge avoidance overrides every other behaviour. Most beginner losses
   are driving yourself off, or stalling (opponent wins if you stop 5 s).
2. **Get under the opponent.** A lower wedge strips their traction → you win the shove.
3. **Weight + grip win pushing.** Pushing is traction-limited; use the full weight allowance,
   kept low and forward, over high-grip wheels.
4. **Find them fast, commit fully.** Efficient search, then full-power ram once detected.
5. **Don't get baited into an orbit** (the rules grant a rematch if two bots circle with no
   progress for 5 s — that resets any advantage). Cut inside and drive *through*.

---

## Phase 1 — Simulator ✅ DONE

`index.html`. Top-down 2D rigid-body physics with traction-limited pushing and a wedge
under-ride model; faithful match rules; paste-in JS bots; per-bot chassis config.

**Simulator architecture** (all in `index.html`, one `<script>`):

- `V` — 2D vector helpers (canvas coords: x right, y down).
- `Bot` — pose, velocity, mass/inertia, chassis cfg, `corners()` polygon.
- `driveBot()` — per-wheel tire model. Each wheel applies an impulse toward its commanded
  longitudinal speed and kills lateral slip, **capped by `µ·N·tractionMod`** → realistic slip
  and pushing.
- `collide()` / `resolve()` — SAT polygon collision + impulse resolution. `resolve()` also
  computes the **wedge under-ride**: if a bot faces the other and has a lower `wedge`, it sets
  the other's `tractionMod < 1`.
- `senseFor()` — IR rays (`rayHitsPoly`), fused `enemy` reading (gated by range + FOV), and
  four corner edge sensors.
- `compile()` / `runBrain()` — wraps user code in a `Function` and runs it each control tick.
- Match controller — `setupRound`, `startMatch`, `step`, `checkRound`, `endRound` (states:
  `idle → countdown → running → roundover → matchwait → matchover`).
- `render()` + rAF loop. UI wiring + localStorage persistence at the bottom.

**Tuning knobs** (top of script): `RING_R`, `BORDER`, `SENSOR_RANGE`, `FOV`, `MOUNT_ANGLES`,
`DT`, `STALL_*`, `ROUND_TIME`, `FREEZE`, and the wedge constant `K` in `resolve()`.

---

## Phase 2 — Strategy bots

Write several distinct bots as standalone JS snippets (one per file under `bots/`) that paste
into the A/B boxes. Each must lead with edge-avoidance. Implement at least:

- **`rammer`** — the baseline: edge-safe, search-spin, charge straight (already the in-app default).
- **`flanker`** — when enemy detected, arc to approach their **side/rear** (where their wedge
  can't engage) instead of head-on; use `enemy.bearing` history in `memory`.
- **`edge-lurer`** — bait near the ring edge, then sidestep so a charging opponent self-outs;
  needs careful edge-sensor logic so it doesn't self-out first.
- **`juker`** — quick feints: dash off-axis at the last moment before contact to slip the
  opponent's wedge, then hit their flank.
- **`spiral-searcher`** — expanding-spiral search that guarantees full-ring coverage fast, then
  commits.

**Acceptance:** each bot beats `rammer` over a tournament (see Phase 3) more often than it loses,
or has a clear documented niche. Keep every bot edge-safe (verify it never self-outs in 100 runs).

## Phase 3 — Headless tournament harness

Single matches are noisy (symmetry → draws). Add a way to run **N matches A-vs-B headlessly**
and report win %, average margin, and self-out rate. Two options:

- In-page "Run 100" button that loops the existing `step()` with rendering off and tallies results, or
- A Node script that imports the pure-physics functions (refactor them out of `index.html` into
  a small `engine.js` shared by both the page and Node).

**Acceptance:** `A vs B, n=100` returns stable win percentages and runs in a few seconds.

## Phase 4 — Realism pass (optional but recommended)

- Add **sensor noise** (jitter on `enemy.distance/bearing`, occasional missed detection) and a
  toggle, so strategies don't overfit to perfect sensing.
- Add **configurable sensor layouts** (number/angles of IR sensors; ultrasonic vs IR range) to
  match what the team actually mounts.
- Add **randomised start positions/headings** to test robustness.
- **Model the 4 starting positions (§3.3 / Figure 2).** The real rules let each team pick one of four
  start positions/orientations before each round (forward = where the main sensor/wedge faces). Add a
  per-bot start-position choice (config or a `chooseStart()` hook returning 0–3) and place bots on
  opposing sides accordingly. **Needs the Figure 2 geometry** — not in the text export; get the image
  from the team before implementing.

## Phase 5 — Hardware firmware port

Port the winning strategy to the real robot. The sim API maps 1:1 to firmware:

| Sim | Hardware (Arduino/C) |
|---|---|
| `enemy` / `sensors` | Sharp IR distance sensors + ultrasonic, fused into bearing/range |
| `edge.fl/fr/bl/br` | downward IR reflectance sensors at the corners (white-line detect) |
| `setMotors(l, r)` | PWM to the two motor drivers (−1…1 → direction + duty) |
| `memory` | global state struct |
| control tick | the `loop()` at a fixed rate |

Respect stream constraints: max 2 driven wheels; Standard = no drivetrain mods; fuse fitted;
no banned mechanisms. Keep the 5 s start delay in firmware.

**Acceptance:** the robot reproduces the simulated behaviour: searches, charges, and — most
importantly — never drives itself out and never stalls.

---

## How to verify changes (any phase)

`index.html` runs a continuous render loop, so screenshot tools that wait for "page idle" will
time out. To drive it headlessly in a browser/devtools console: call `startMatch()` then loop
`step()` and read `state`, `scoreA`, `scoreB`, `A.pos`, `B.pos`. Asymmetric configs should give
decisive results (a low/heavy wedge reliably beats a high/light one by push-out); identical bots
should mostly draw.
