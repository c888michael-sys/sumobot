# Build plan — SUMOBOTS

Roadmap for finishing the project. **Phase 1 (the simulator) is done.** The rest is written so a
fresh agent (e.g. Claude Sonnet) or a human can execute it **cold**, with no other context. Build
against the simulator first, then port to hardware.

**How to use this doc:** read *Invariants & gotchas* and the *Bot API contract* first — they prevent
the most common mistakes. Then do the phases **in order** (2 before 3: the harness must exist before
bots can be scored). Each phase lists explicit deliverables and a "done when" check.

---

## Context

RAMSoc's **SUMOBOTS**: autonomous robot sumo, push the opponent out of a 120 cm ring. No remote
control. 5 s start freeze. Best-of-3, 2 min rounds. A robot that stops moving > 5 s loses. Streams:

- **Standard** — UNSW only. ≤ 200 mm × 200 mm, ≤ 1 kg. Up to 4 IR + 4 ultrasonic. Max 2 driven
  wheels, no drivetrain mods. 1–5 A fuse.
- **Open** — anyone. ≤ 250 mm × 250 mm, ≤ 1.5 kg. $200 budget ($100 on the RAMSoc base kit).
  Unrestricted drivetrain (still max 2 driven wheels). 1–10 A fuse.

Banned: remote control, IR jamming, weapons, liquids/powders/gases, throwing, traction adhesives,
magnets/vacuum, sharp edges (radius must exceed 0.005″). Full summary in [RULES.md](RULES.md).

## Competitive meta (what the bots should embody)

1. **Never self-out.** Edge avoidance overrides everything. Most losses are self-outs or stalls.
2. **Get under the opponent.** Lower wedge strips their traction → you win the shove.
3. **Weight + grip win pushing.** Pushing is traction-limited; use full weight, low and forward.
4. **Find fast, commit fully.** Efficient search, then full-power ram.
5. **Don't get baited into an orbit** (rules grant a rematch on no-progress circling — resets your
   advantage). Cut inside and drive *through*.

---

## Invariants & gotchas (read before touching code)

- **One file.** Everything is in `index.html` (one `<script>`). No build step, no dependencies.
- **Units:** centimetres and cm/s. Ring radius `RING_R = 60`. Border `BORDER = 2.5`. Control tick
  `DT = 1/120` s. Freeze `FREEZE = 5` s. Round `ROUND_TIME = 120` s. Stall = speed < `STALL_SPEED`
  (4 cm/s) for `STALL_TIME` (5 s).
- **Coordinates:** canvas-style — **x right, y DOWN**, origin at ring centre. Heading `theta`: 0 = +x,
  `π/2` = +y (down), `π` = −x. `fwd = (cos θ, sin θ)`.
- **Determinism:** the engine uses **no randomness**. The same two bots + same start produce the
  **exact same match every time.** ⇒ a tournament is meaningless unless you *inject variance*
  (see Phase 2). Do not skip this — it is the most common mistake.
- **`memory` resets every round** (`Bot.place()` sets `memory = {}`). It persists across ticks
  *within* a round only. Don't rely on cross-round memory.
- **Strategy vs chassis boundary:** bot **code** only ever calls `setMotors()` (+ reads sensors).
  Mass / wedge height / grip / size / max speed are **chassis config** (the dropdowns + number
  fields), *not* something code sets. A "low wedge" strategy = set `wedge` low in config, not in code.
- **Headless run gotchas** (you WILL need these for Phase 2):
  - Set `running = false` first, or the `requestAnimationFrame` loop also calls `step()` → double steps.
  - `step()` only advances when `state` is `countdown`/`running`/`roundover`/`matchwait`. Call
    `startMatch()` to begin (it compiles the brains and enters `countdown`).
  - A match only reaches `state === 'matchover'` when a bot wins **2 rounds**. Evenly-matched bots
    draw rounds forever (the sim has no "3 ties → decision" yet). **Always loop with an iteration cap**
    and a **max-rounds guard**, then decide by score. (Adding the max-rounds rule is a Phase 2 task.)
- **Verifying in the browser:** the page runs a continuous render loop, so "wait for idle" screenshot
  tools time out. Drive it from the devtools/`preview_eval` console instead: `running=false;
  startMatch(); while(state!=='matchover' && i++<24000) step();` then read `scoreA/scoreB/A.pos/B.pos`.

---

## Bot API contract (authoritative)

User code is the body of a function run every control tick (~120 Hz). It receives these in scope
(this is the real signature in `compile()` — keep them in sync if you change it):

```
(enemy, sensors, edge, time, frozen, memory, setMotors, log, start)
```

| Name | Type | Meaning |
|---|---|---|
| `enemy` | `{detected:bool, distance:cm, bearing:deg}` | Fused opponent reading. `bearing` is signed: **+ = to your right, − = left, 0 = dead ahead**. `detected` is true if any IR ray hits **or** the opponent is within `SENSOR_RANGE` (80 cm) and inside ±`FOV` (75°). |
| `sensors` | `[{angle:deg, distance:cm}]` | Raw IR rays at mount angles `[-40,-15,0,15,40]` (+ = right). `distance = Infinity` when the ray hits nothing. |
| `edge` | `{fl,fr,bl,br}` (bool) | Corner line sensors — `true` when that corner is over the white border (radius > `RING_R - BORDER` = 57.5 cm). `f`=front, `b`=back, `l`=left, `r`=right. |
| `time` | number (s) | Seconds since the freeze ended. **Negative during the 5 s countdown.** |
| `frozen` | bool | `true` during the countdown. Motors are ignored while frozen, but **sensors are live** — plan into `memory`. |
| `memory` | object | Your RAM. Persists across ticks within a round; **reset each round**. |
| `setMotors(l, r)` | fn | Drive. Each wheel `−1…1` (clamped). Differential drive: `(1,1)` = forward, `(1,-1)` = spin right, `(-1,-1)` = reverse. |
| `log(...)` | fn | Print to the console panel (throttled). |
| `start` | int `1–4` | Which Figure-2 start position you chose. **Pos 1 faces *outward*** (away from the opponent) — a bot starting there must turn ~180° first. Pos 4 is head-on. Branch on this if useful. |

**Chassis config** (per bot, set in the UI, stored in `cfgA`/`cfgB`): `mass` (kg), `len`/`wid` (cm),
`wedge` (0–1, lower = gets under), `mu` (grip), `maxSpeed` (cm/s), plus `start` position (1–4).

**A bot is just the text** you paste into the Bot A / Bot B box. Store each strategy as a file under
`bots/<name>.js` whose **entire contents are that paste-ready body** (no wrapper function, no exports).

---

## Phase 1 — Simulator ✅ DONE

`index.html`: top-down 2D rigid-body physics, traction-limited pushing, wedge under-ride model,
faithful match rules, four Figure-2 start positions, paste-in JS bots, per-bot chassis config.

**Architecture** (all in the one `<script>`):
- `V` — 2D vector helpers.
- `Bot` — pose, velocity, mass/inertia, chassis cfg, `corners()` polygon, `place()`, `applyCfg()`.
- `driveBot()` — per-wheel tire model; each wheel drives toward its commanded longitudinal speed and
  kills lateral slip, **capped by `µ·N·tractionMod`** → realistic slip + pushing.
- `collide()` / `resolve()` — SAT polygon collision + impulse resolution. `resolve()` also computes
  the **wedge under-ride**: a bot facing the other with a lower `wedge` sets the other's
  `tractionMod < 1` (constant `K`).
- `senseFor()` — IR rays (`rayHitsPoly`), fused `enemy`, four corner edge sensors.
- `compile()` / `runBrain()` — wraps user code in a `Function`, runs it each tick.
- `START_A` / `START_B` + `setupRound()` — the four start positions (mirrored) and round setup.
- Match controller — `startMatch`, `step`, `checkRound`, `endRound`; states
  `idle → countdown → running → roundover → (matchwait) → matchover`.
- `render()` + rAF loop; UI wiring + `localStorage` persistence at the bottom.

**Tuning knobs** (top of script): `RING_R`, `BORDER`, `SENSOR_RANGE`, `FOV`, `MOUNT_ANGLES`, `DT`,
`STALL_*`, `ROUND_TIME`, `FREEZE`, the wedge constant `K` in `resolve()`, and `SIDEPUSH` (grip a
flank-hit bot keeps; `1.0` = front-to-side push off). Measured: `SIDEPUSH≈0.25` is the sweet spot —
it makes matches decisive without being the thing that suppresses strategy (see
[bots/README.md](bots/README.md)); strategy diversity is gated by arena/sensing, not by `SIDEPUSH`.

---

## Phase 2 — Headless tournament harness ✅ DONE

Implemented in `index.html`: the **⚔ Run** button + match count runs `runMatches(n)` headlessly and
prints win % to the console. It injects variance (random start positions + heading `JITTER`), is
**side-balanced** (alternates which slot each bot plays to cancel slot/left-right bias), and has the
`MAX_ROUNDS` guard so endless draws can't loop. The spec below records how/why.

Single matches are deterministic and noisy at symmetry, so scoring needs many *varied* matches.

**2a. Add a max-rounds guard** so evenly-matched bots can't loop forever. In `checkRound()`/round
flow: if `round` exceeds e.g. 7 with no 2-round winner, end the match as a **draw** (set
`state='matchover'`, winner = higher score or draw).

**2b. Inject variance** (required — see determinism gotcha). Add a tournament-only randomisation,
e.g. a `randomizeStart()` that, before each match, sets `startA`/`startB` to a random 1–4 **and**
jitters each start heading by ±15° (add a small `theta += (rand-0.5)*0.5` in `setupRound`, gated by a
`JITTER` flag so single interactive matches stay clean). Optionally seedable for reproducibility.

**2c. Run loop.** Add a "Run N (headless)" button + count input. Reference implementation:

```js
function runMatches(n){
  running = false;                          // stop rAF double-stepping
  let a=0, b=0, draws=0;
  for(let m=0; m<n; m++){
    randomizeStart();                        // variance — or every match is identical
    startMatch();                            // compile + countdown
    let i=0;
    while(state!=='matchover' && i<200*120){ step(); i++; }  // hard cap ~200s
    if(scoreA>scoreB) a++; else if(scoreB>scoreA) b++; else draws++;
  }
  return {a, b, draws, winPctA:(a/n*100).toFixed(1)};
}
```

Report **win % A / win % B / draw %** (extend to track avg margin and self-out rate if useful).
Keep it in-page (reuse `step()`); **do not** refactor the physics into a separate `engine.js` unless
you first snapshot the working file — that refactor is the highest-risk change in this plan.

**Done when:** `runMatches(100)` for two *different* bots returns **non-identical, stable** win
percentages and finishes in a few seconds; identical bots return ≈ 50/50 (within noise) plus draws.

## Phase 3 — Strategy bots ✅ DONE

Five bots live in `bots/` (`rammer`, `spiral-searcher`, `flanker`, `juker`, `counter-puncher`), each a
paste-ready body with the edge-guard preamble, loadable via the in-app dropdown. **Measured finding
(see [bots/README.md](bots/README.md)): at equal chassis, none reliably beats a competent `rammer`** —
the front-to-side push makes turning broadside fatal, so flanking/juking lose and head-on aggressors
draw; the real lever is chassis (wedge/weight), not strategy. They're kept as a teaching set that
documents *why* the clever moves underperform. Spec below.

Write each as a paste-ready `bots/<name>.js` (body only). **Every bot must start with the same
edge-guard preamble** (copy from the in-app default bot) so it never self-outs. Implement at least:

- **`rammer`** — baseline: edge-safe, spin-search, charge straight (= current in-app default).
- **`flanker`** — on detect, arc to the opponent's **side/rear** (where their wedge can't engage)
  using `enemy.bearing` history in `memory`, instead of charging head-on.
- **`edge-lurer`** — bait near the rim, sidestep so a charging opponent self-outs; needs very careful
  edge logic so it doesn't self-out first.
- **`juker`** — feint: dash off-axis just before contact to slip the opponent's wedge, then hit a flank.
- **`spiral-searcher`** — expanding-spiral search for fast full-ring coverage, then commit.

**Optional loader** (nice-to-have): add a per-bot dropdown that does
`fetch('bots/'+name+'.js').then(r=>r.text()).then(t=>codeX.value=t)` from a `bots/manifest.json`.
Note this needs the page **served over http** (the `sumo-sim` launch config), not opened as `file://`.

**Done when:** each bot, scored by the Phase 2 harness over n≥100, **beats `rammer` more than it
loses** *or* has a documented niche; and **self-outs in 0 of 100** runs.

## Phase 4 — Realism pass (optional but recommended)

- **Sensor noise** — jitter `enemy.distance/bearing`, occasional missed detection; behind a toggle so
  strategies don't overfit to perfect sensing.
- **Configurable sensor layouts** — number/angles of IR rays; ultrasonic vs IR range — to match the
  real mounted hardware.
- ~~**Model the 4 starting positions (§3.3 / Figure 2).**~~ ✅ **DONE** in Phase 1. Each bot picks one
  of four spots (position + heading) via a dropdown; exposed to code as `start` (1–4); drawn as faint
  numbered markers. Geometry in `START_A`/`START_B`. Headings: 1 inner/faces-out, 2 top/angled-in,
  3 bottom/angled-in, 4 outer/head-on (B mirrored).

## Phase 5 — Hardware firmware port

Port the winning strategy to the robot. The sim API maps 1:1 to firmware:

| Sim | Hardware (Arduino/C) |
|---|---|
| `enemy` / `sensors` | Sharp IR distance + ultrasonic, fused into bearing/range |
| `edge.fl/fr/bl/br` | downward IR reflectance sensors at the corners (white-line detect) |
| `setMotors(l, r)` | PWM to the two motor drivers (sign → direction, magnitude → duty) |
| `memory` | a `static` state struct |
| control tick | `loop()` at a fixed rate |

Skeleton translating the default bot (pseudo-Arduino; fill in pins/drivers for the team's BOM):

```c
void loop() {
  Edge e = readEdges();                 // 4 reflectance sensors vs white-line threshold
  if (millis() - startMs < 5000) { drive(0,0); return; }   // 5 s freeze

  if (e.fl && e.fr)      { drive(-1,-1); return; }          // self-preservation first
  else if (e.fl)         { drive(0.6,-1); return; }
  else if (e.fr)         { drive(-1,0.6); return; }
  else if (e.bl || e.br) { drive(1,1);  return; }

  Enemy en = senseEnemy();              // fuse IR + ultrasonic → detected/bearing
  if (en.detected) {
    if (en.bearing >  12) drive(1, 0.15);
    else if (en.bearing < -12) drive(0.15, 1);
    else drive(1, 1);                   // charge
    return;
  }
  drive(0.55, -0.55);                   // search spin
}
```

Respect stream constraints: max 2 driven wheels; Standard = no drivetrain mods; fuse fitted (1–5 A
Standard / 1–10 A Open); no banned mechanisms; keep the 5 s start delay.

**Done when:** on the real ring the robot searches, charges, and — above all — **never self-outs and
never stalls**.

---

## Feature — Keyboard control mode (W/S, A/D spin, ←/→ swing)

Add a third per-bot control mode `'keys'` alongside the existing `'code'`/`'mouse'` (same `ctrlA`/
`ctrlB` plumbing). The bot whose control = `keys` is driven by the keyboard. Differential drive with
**two kinds of turn** (because the wheels sit at the sides, not the centre):

- **`W` / `S`** — forward / reverse (both motors together).
- **`A` / `D` — spin (point) turn:** both motors fire opposite ways → rotates about the bot's
  **centre**, no translation. The *fast* turn.
- **`←` / `→` — swing turn:** **one** motor fires, the other is **held still** → rotates about the
  **planted (stationary) wheel** and creeps forward at half speed. The *gentle, wider* turn.

**Verified physics:** the spin turn's yaw rate is exactly **2× the swing turn's** (yaw rate
`ω = (vR − vL)/w`, track width `w`; spin differential `= 1−(−1) = 2`, swing `= 1−0 = 1`). Measured in
the sim at 2.00× across 40/80/160 cm/s. So `←`/`→` give finer aiming; `A`/`D` snap around twice as
quick. *(Line numbers below are approximate — anchor on the function/selector names.)*

1. **Dropdown option** — add to both `<select data-ctrl="A">` and `<select data-ctrl="B">`:
   ```html
   <option value="keys">⌨ keys — W/S · A·D spin · ←→ swing</option>
   ```

2. **Key state + listeners** (place near the mouse pointer handlers):
   ```js
   const keys = {};
   const KEYNAMES = ['w','a','s','d','arrowleft','arrowright'];
   const keysActive = () => ctrlA === 'keys' || ctrlB === 'keys';
   window.addEventListener('keydown', e => {
     if (!keysActive()) return;
     const tag = document.activeElement && document.activeElement.tagName;
     if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return; // don't hijack editor typing
     const k = e.key.toLowerCase();
     if (KEYNAMES.includes(k)) { keys[k] = true; e.preventDefault(); }      // preventDefault also stops arrow-key scroll
   });
   window.addEventListener('keyup', e => { const k = e.key.toLowerCase(); if (k in keys) keys[k] = false; });
   ```

3. **Drive function** (next to `manualDrive`):
   ```js
   function keyDrive(){
     let l = (keys.w ? 1 : 0) - (keys.s ? 1 : 0);      // W/S: both motors (forward / reverse)
     let r = l;
     const spin = (keys.d ? 1 : 0) - (keys.a ? 1 : 0); // A/D: spin in place, opposite motors, +1 = right
     l += spin; r -= spin;
     if (keys.arrowright) l += 1;                      // swing right: fire LEFT motor, right held still
     if (keys.arrowleft)  r += 1;                      // swing left:  fire RIGHT motor, left held still
     return [clamp(l, -1, 1), clamp(r, -1, 1)];
   }
   ```
   Pure presses: `D`→`[1,-1]` spin right, `A`→`[-1,1]` spin left, `→`→`[1,0]` swing right (pivots on the
   right wheel), `←`→`[0,1]` swing left, `W`→`[1,1]`. Arrows already creep forward, so use them on their
   own — combining with `W` cancels the "held still" wheel.

4. **Wire into `step()`** — extend the two motor-assignment lines:
   ```js
   const [al,ar] = ctrlA==='mouse' ? (frozen?[0,0]:manualDrive(A,A.target))
                 : ctrlA==='keys'  ? (frozen?[0,0]:keyDrive())
                 : runBrain(A,brainA,sA,frozen);
   const [bl,br] = ctrlB==='mouse' ? (frozen?[0,0]:manualDrive(B,B.target))
                 : ctrlB==='keys'  ? (frozen?[0,0]:keyDrive())
                 : runBrain(B,brainB,sB,frozen);
   ```
   (Keeps the freeze behaviour — a keys-bot holds still during the 5 s countdown.)

5. **Clear held keys** on mode switch (in the `data-ctrl` `onchange`) and at round start (in
   `setupRound`): `for (const k in keys) keys[k] = false;` — avoids a "stuck key" after switching.

6. **Headless guard** (cosmetic) — widen the `runN` warning to
   `if (ctrlA!=='code' || ctrlB!=='code') logLine('sys','(manual/keyboard bots play as code in headless runs)','sys');`.
   `runMatches` already forces `'code'`, so no further change needed.

- **Persistence:** none needed — `ctrlA`/`ctrlB` already save/load; `'keys'` flows through.
- **Docs:** add a line to the help/API panel ("⌨ keys: W/S drive · A/D spin in place (fast) · ←/→
  one-wheel swing (half as fast)") and to README.md's feature list.
- **One keys-bot at a time.** Both A and B in `keys` mode would share the same keys and move
  identically. Arrows are now the swing turn, so for local 2-player you'd give Bot B a *different*
  cluster (e.g. `I`/`K` drive, `J`/`L` spin) via a second key set — not in scope here.

**Done when:** with Bot A control = `keys`: `W` drives forward, `A`/`D` spin in place, `←`/`→` do a
slower one-wheel swing (~half the yaw rate, pivoting on the planted wheel), nothing moves during the
5 s freeze, arrow keys don't scroll the page, and typing in the code boxes still works (keys not
hijacked).

---

## Feature — Match recorder: movement trace + path diagram of winning rounds

Auto-record every round; when a bot **wins**, keep that round so you can review *how* it won without
a video: (a) a **movement trace** — a timestamped, human-readable list of the winner's moves with
magnitudes (e.g. `t=0.30  turn right 41°`, `t=0.55  forward 18 cm`, `t=0.80  side hit → push-out`),
and (b) a **path diagram** — both bots' trajectories drawn on the ring (start → end, with heading and
the contact / push-out points). Plus **JSON export**. Store actual per-tick frames (poses + motor
commands + sensor state) as the data source — works for `code`, `mouse`, and `keys` bots.
*(Line numbers approximate — anchor on the function names.)*

**State** (near the other globals):
```js
let recording = true;     // master switch — FORCE false inside runMatches (don't record tournaments)
let rec = [];             // current round's frames
let replays = [];         // saved winning rounds (newest first, cap ~10)
const REC_EVERY = 2;      // record every Nth control tick (~60 Hz) to halve memory; 1 = every tick
let recTick = 0;
let view = 'live';        // 'live' | 'diagram' — when 'diagram', the arena shows a saved win's path
let shownReplay = null;   // the replay currently drawn as a path diagram
const anyEdge = e => e.fl || e.fr || e.bl || e.br;
```

1. **Capture a frame each running tick** — in `step()`, inside the `state==='running'` path, *after*
   `al/ar/bl/br` are computed and `sA/sB` exist:
   ```js
   if (recording && state==='running' && (recTick++ % REC_EVERY === 0)) {
     rec.push({
       t:+t.toFixed(2),
       a:[+A.pos.x.toFixed(1),+A.pos.y.toFixed(1),+A.theta.toFixed(3)],
       b:[+B.pos.x.toFixed(1),+B.pos.y.toFixed(1),+B.theta.toFixed(3)],
       am:[+al.toFixed(2),+ar.toFixed(2)], bm:[+bl.toFixed(2),+br.toFixed(2)],
       ae: sA.enemy.detected ? [Math.round(sA.enemy.bearing),Math.round(sA.enemy.distance)] : null,
       be: sB.enemy.detected ? [Math.round(sB.enemy.bearing),Math.round(sB.enemy.distance)] : null,
       aE:anyEdge(sA.edge), bE:anyEdge(sB.edge),
       sh:(A._sideHit?'A':0)||(B._sideHit?'B':0)||0   // see step 4 (optional)
     });
     if (rec.length>4000) rec.shift();                 // memory backstop for very long rounds
   }
   ```

2. **Reset the buffer at round start** — in `setupRound()`: `rec=[]; recTick=0;`

3. **Save on win** — in `endRound(winner,reason)`, when `winner` is `'A'`/`'B'`:
   ```js
   if (winner && recording) {
     replays.unshift({ winner, reason, round, when:Date.now(),
       meta:{ nameA:A.name, nameB:B.name, ctrlA, ctrlB, startA, startB,
              cfgA:{...cfgA}, cfgB:{...cfgB}, ring:RING_R },
       frames: rec.slice() });
     if (replays.length>10) replays.length=10;
     logLine('sys',`Recorded win — ${winner==='A'?A.name:B.name} (replay available)`,'sys');
     refreshReplayList();                              // repopulate the <select> (step 9)
   }
   ```
   (Draws are skipped — the request is "when one bot beats another".)

4. **(Optional) side-hit event** — in `resolve()`, clear `A._sideHit=B._sideHit=false` at the top, and
   where the front-to-side push fires set the victim's flag (`B._sideHit=true` / `A._sideHit=true`).
   Lets the trace mark "side hit on the flank". Skip if you'd rather not touch `resolve()`.

5. **Gate OFF in tournaments** — in `runMatches()`, save+set `recording=false` at the start and restore
   it at the end (otherwise it allocates frames for all N matches).

6. **Path diagram (static — no playback loop).** When a saved win is selected, draw its trajectories
   on the arena instead of the live world:
   - Set `view='diagram'; shownReplay=r;`. In the `frame()` loop, when `view==='diagram'` call
     `renderDiagram(shownReplay)` instead of the live `render()` (and skip the physics `step()`).
   - `renderDiagram(r)`: draw the ring (factor the ring-draw out of `render()` so both reuse it), then
     for each bot draw a **polyline** through its recorded positions (`frames[].a` / `.b`) in the A/B
     colour; mark **start** (hollow circle), **end** (filled dot + short heading arrow), a small dot
     every ~1 s of `t`, and the **side-hit / push-out** points (from `sh` and the final frame). A
     light caption shows winner + reason. No animation — one static draw.

7. **Movement trace (with magnitudes).** Derive a readable log of the **winner's** moves from
   `frames` — one line per *move segment*, not per tick:
   - Classify each frame by motor cmd: both≈equal&>0 `forward`, both≈equal&<0 `reverse`, `l>r`
     `turn/arc right`, `l<r` `turn/arc left`, ≈0 `idle`. Group consecutive same-class frames into a
     segment.
   - For each segment compute, from the poses: net **Δheading** (deg, sign → left/right) and net
     **distance** (cm), plus its start `t` and duration. Emit e.g. `t=0.30  turn right 41°  (0.25s)`,
     `t=0.55  forward 18 cm`, or if both are significant `t=0.55  arc right 22°, forward 14 cm`.
     (Δheading from `theta` deltas — unwrap across ±π; distance from summed `pos` deltas.)
   - Interleave **event** lines by time: enemy detected/lost, `sh` side hit, first contact, and the
     final `t=…  push-out → WIN`.
   - Render into a scrollable `#trace` panel. Nice-to-have: hovering a line highlights that point on
     the path diagram (store each line's frame index). Add a toggle to show the **loser's** trace too.

8. **Export** — `⤓ Export JSON` downloads the selected recording (`frames` + `meta` + the derived
   trace; local download of the user's own data — fine):
   ```js
   const r=replays[sel];
   const blob=new Blob([JSON.stringify(r)],{type:'application/json'});
   const a=document.createElement('a'); a.href=URL.createObjectURL(blob);
   a.download=`sumo-win-${r.winner}-${r.when}.json`; a.click();
   ```
   Nice-to-have: also export the **path diagram as PNG** (`arena.toDataURL('image/png')` while the
   diagram is shown) and the trace as a `.txt`.

9. **UI** — add a collapsible **"Replays"** card under the Console (left column): a
   `<select id="replayList">` of saved wins (`"Bot B · round 2 · 0:05 · just now"`); selecting one
   sets `view='diagram'`, draws its path on the arena, and fills the scrollable `#trace` panel. Add a
   `⤓ Export` button and a **"← back to live"** button (sets `view='live'`). `refreshReplayList()`
   repopulates the select. Run/Reset also set `view='live'` (stored frames are independent of live
   `A`/`B`, so the live match is untouched).

- **Docs:** add a line to README.md's feature list and the help panel.

**Done when:** play a code-vs-code match (or drive Blue by mouse/keys) to a win → a Replays entry
appears; selecting it draws both bots' **paths** on the arena and fills the **movement trace** with
timestamped moves and magnitudes (`turn right 41°`, `forward 18 cm`, …) ending in the push-out;
⤓ Export downloads a JSON; "back to live" restores the live arena; and a headless `⚔ Run` tournament
records nothing (no memory growth).

---

## Feature — Split wedge: `wedgeL` / `wedgeR` (asymmetric, per contact point)

Replace the single `wedge` scalar with **two** per-bot heights — `wedgeL` (left edge) and `wedgeR`
(right edge) — and make the under-ride use the height **at the actual contact point**, not a global
scalar. This lets a bot be low on one side and higher on the other, so *which part of your wedge
touches* decides whether you get under. (Build this BEFORE the presets feature, which uses it.)
*(Line numbers approximate — anchor on the function names.)*

1. **Config fields** — in `applyCfg()` read `wedgeL`/`wedgeR` (clamp 0–1). Keep a derived
   `this.cfg.wedge = (wedgeL+wedgeR)/2` so existing references (e.g. nose rendering) don't break.
   **Migration:** if a cfg/save has only old `wedge`, set `wedgeL=wedgeR=wedge`.

2. **Config UI** — replace the single "wedge height 0–1" input (both `data-bot`) with two:
   `wedge L 0–1` (`data-k="wedgeL"`) and `wedge R 0–1` (`data-k="wedgeR"`), default `0.5`/`0.5`.

3. **Effective height at a contact point** (add near `resolve()`):
   ```js
   function wedgeAt(bot, cp){
     const u = clamp(V.dot(V.sub(cp, bot.pos), bot.right()) / (bot.cfg.wid/2), -1, 1); // -1 left .. +1 right
     return bot.cfg.wedgeL + (bot.cfg.wedgeR - bot.cfg.wedgeL) * (u + 1) / 2;
   }
   ```

4. **Use it in the under-ride** — in `resolve()`, after the contact `c` exists, swap the scalar
   `A.cfg.wedge`/`B.cfg.wedge` in the wedge block for `wedgeAt(A, c.cp)` / `wedgeAt(B, c.cp)`:
   ```js
   const wA = wedgeAt(A, c.cp), wB = wedgeAt(B, c.cp);
   if (faceA>0.3 && wA < wB) B.tractionMod = Math.min(B.tractionMod, strip(wA, wB));
   if (faceB>0.3 && wB < wA) A.tractionMod = Math.min(A.tractionMod, strip(wB, wA));
   ```
   (`faceA>0.3`/`faceB>0.3` already gate to front engagement. Leave the `SIDEPUSH` block unchanged.)

5. **Rendering** — `drawBot()` shades the nose by `cfg.wedge`; make it reflect the tilt: shade the
   **lower** side brighter (e.g. a left→right brightness gradient across the nose, or a short bright
   tick on whichever of L/R is lower). Optional but makes asymmetry visible.

6. **Persistence** — save/load `wedgeL`/`wedgeR` (with the old-`wedge` migration in step 1).

- **Harness:** automatic — the ⚔ tournament already compares whole configs.
- **Honest caveat (keep in mind, not a bug):** still 2D — this captures "which side gets under," not
  true 3D ride-up/lift, and there's no wedge-floor drag, so the model still favours "as low as
  possible on the contact side." Treat results as directional.

**Done when:** give Bot A `wedgeL=0.05`, `wedgeR=0.30` vs a symmetric `0.5/0.5` Bot B; the under-ride
now depends on where contact lands (A wins when it lands left-side contact, loses when it presents its
high right side), and the ⚔ harness reflects it.

## Feature — Wedge under-ride consequence: ride-up overlap + speed penalty

Today the under-ride only strips the upper bot's traction (`strip()`), but the collision still pushes
both bots apart **equally**, so the lower wedge never visibly *gets under* and the consequence is weak.
Add the missing physical consequence: when one bot's wedge is lower at the contact, it **rides under**
(gains overlap / keeps advancing) and the upper bot gets a **speed/push penalty** — both scaled by the
wedge-height gap. All changes are in `resolve()`. *(Line numbers approximate.)*

1. **Compute who's under whom first.** Move the wedge-height/face calc *above* the positional
   correction and derive a `lift` (0–1) per bot from the height gap (`liftB>0` ⇒ B is on top):
   ```js
   const dirAB=V.norm(V.sub(B.pos,A.pos));
   const faceA=V.dot(A.fwd(),dirAB), faceB=V.dot(B.fwd(),V.mul(dirAB,-1));
   const wA=wedgeAt(A,c.cp), wB=wedgeAt(B,c.cp);
   const RIDE=2.0;                                  // how strongly a height gap converts to ride-up (tune)
   let liftA=0, liftB=0;
   if(faceA>0.3 && wA<wB) liftB=clamp((wB-wA)*RIDE,0,1);   // A got under -> B is lifted
   if(faceB>0.3 && wB<wA) liftA=clamp((wA-wB)*RIDE,0,1);
   const lift=Math.max(liftA,liftB);
   ```

2. **Overlap** — bias the positional correction onto the *lifted* bot so the lower one advances into it
   (instead of the current equal `invMass` split):
   ```js
   const tot=A.invMass+B.invMass, corr=Math.max(depth-0.05,0)*0.8;
   let shareA=A.invMass/tot, shareB=B.invMass/tot;
   if(liftB>0){ shareB+=shareA*liftB; shareA*=(1-liftB); }  // A under -> A barely pushed back -> overlaps B
   if(liftA>0){ shareA+=shareB*liftA; shareB*=(1-liftA); }
   A.pos=V.sub(A.pos,V.mul(n,corr*shareA));
   B.pos=V.add(B.pos,V.mul(n,corr*shareB));
   ```

3. **Less bounce while engaged** — make the normal impulse inelastic when under-riding so the lower bot
   stays locked on and shoves rather than bouncing off. Replace the fixed restitution `-(1.0)*vn` with
   `-(1.0-0.9*lift)*vn`.

4. **Speed/push penalty on the upper bot** — keep the existing `strip()` traction cut, and add a direct
   velocity damp on whoever is lifted:
   ```js
   if(liftB>0){ B.tractionMod=Math.min(B.tractionMod,strip(wA,wB)); B.vel=V.mul(B.vel,1-0.2*liftB); }
   if(liftA>0){ A.tractionMod=Math.min(A.tractionMod,strip(wB,wA)); A.vel=V.mul(A.vel,1-0.2*liftA); }
   ```
   (This replaces the old two `strip()` lines.)

- **Tuning:** `RIDE` (2.0), the velocity damp (0.2), and the restitution drop (0.9) are starting values.
  Tune in the ⚔ harness so a **lower wedge clearly out-shoves a higher one** (bigger win margin than
  today) **without** being an instant game-over, and identical wedges stay ~50/50.
- **Stability caveat:** letting the lower bot overlap is a 2D stand-in for 3D ride-up — bots will visibly
  overlap a little while one is under. Keep the `depth-0.05` deadband so they can't tunnel through each
  other; if two bots ever get stuck interpenetrating, cap `liftA/liftB` lower.
- **Render (optional):** draw the lower (under) bot *after* the upper one so its nose visibly tucks
  beneath — reinforces the "got under" read.

**Done when:** a low-wedge bot vs a high-wedge bot now visibly drives *under* and pushes the high one
out with a clear margin (and the upper bot is slowed while ridden), the effect scales with the wedge-
height gap, identical wedges still draw ~50/50, and no two bots get stuck overlapping.

> **Status:** the ride-up consequence (overlap bias, inelastic impulse, traction strip + velocity
> damp) is **implemented** in `resolve()`. The `fxLog()` effect logger is also implemented — it prints
> "▲ A is under B → B weakened…" / flank-hit lines to the console during live matches (throttled,
> off during headless runs). The turning extension below is the remaining piece.

## Feature — Wedge turning: lever the opponent (off-centre impulse)

Builds on the ride-up consequence. Right now getting under only **pushes and slows** the opponent — it
doesn't **turn** them. Model the wedge as a **2-component effect**: a forward push that, applied at the
**contact point** (off the lifted bot's centre — especially with an asymmetric one-side wedge),
produces a **torque that rotates the opponent**. That enables the "lever them around to expose a flank"
maneuver: get under their left → spin them → their side swings toward you → push out. All in `resolve()`,
after `lift`/`cp` are known. *(Line numbers approximate.)*

```js
const TURN = 0.6;                                   // lever strength (tune)
if (liftB > 0) {                                    // A is under B → lever B
  const drive = Math.max(0, V.dot(A.vel, A.fwd())); // how hard A is driving forward
  const J = V.mul(A.fwd(), TURN * liftB * drive * B.mass);
  applyImpulse(B, J, V.sub(cp, B.pos));             // off-centre contact → cross(r,J) torque turns B
  fxLog('turnB', '↻ A levers B around (turning)');
}
if (liftA > 0) {
  const drive = Math.max(0, V.dot(B.vel, B.fwd()));
  const J = V.mul(B.fwd(), TURN * liftA * drive * A.mass);
  applyImpulse(A, J, V.sub(cp, A.pos));
  fxLog('turnA', '↻ B levers A around (turning)');
}
```

- The torque is automatic — `applyImpulse` already adds `V.cross(r, J) * invI` to omega. With a
  low-**left** asymmetric wedge the contact sits left of the opponent's centreline, so the push spins
  them, swinging a flank toward you (which then feeds the existing `SIDEPUSH`).
- **Tune `TURN`** so a lifted opponent visibly rotates but isn't launched/spun wildly; verify in the
  ⚔ harness that a one-side-low wedge + the `wedge-lever` bot now turns-then-pushes opponents out more
  often than before. Cap it (and `liftA/liftB`) if motion gets chaotic.
- It's the off-centre application that matters — a centre-line hit barely turns, an edge hit turns hard,
  which is exactly the "asymmetric wedge turns the opponent" behaviour you want.

**Done when:** a bot that gets under one side of the opponent visibly **rotates** them (you can lever a
high-wedge bot sideways), the `↻ levers` line shows in the log, and combined with the side-push it can
turn-then-shove them out; identical/symmetric engagements don't spin unnaturally.

## Feature — Chassis presets (incl. "Prototype side wedge")

One-click named chassis setups (config + optional bot code + start position), so you can load a whole
design instead of dialling fields by hand.

1. **Preset data** — an inline `PRESETS` array (kept in `index.html` so it works on `file://` too):
   ```js
   const PRESETS = [
     { name:'Default (symmetric)', cfg:{ mass:1, len:20, wid:20, wedgeL:0.5, wedgeR:0.5, mu:1, maxSpeed:80 } },
     { name:'Low wedge rammer', cfg:{ mass:1, len:20, wid:20, wedgeL:0.1, wedgeR:0.1, mu:1.1, maxSpeed:80 }, bot:'rammer.js' },
     { name:'Prototype side wedge', cfg:{ mass:1, len:20, wid:20, wedgeL:0.05, wedgeR:0.30, mu:1.0, maxSpeed:80 },
       bot:'wedge-lever.js', start:4 },   // low LEFT corner + the wedge-lever maneuver
   ];
   ```

2. **UI** — add a per-bot **preset** `<select data-preset="A">` / `data-preset="B"` in each bot's
   config column (top, near the control/start selects), populated from `PRESETS` (first option:
   `"preset…"`).

3. **Apply** — on change, `applyPreset(which, p)`:
   ```js
   Object.assign(which==='A'?cfgA:cfgB, p.cfg);
   // reflect into the number inputs for that bot:
   document.querySelectorAll(`input[data-k][data-bot="${which}"]`).forEach(inp=>{
     const t = which==='A'?cfgA:cfgB; if (inp.dataset.k in t) inp.value = t[inp.dataset.k];
   });
   if (p.start){ if(which==='A')startA=p.start; else startB=p.start;
     const s=document.querySelector(`select[data-start="${which}"]`); if(s) s.value=p.start; }
   if (p.bot) loadBotInto(which, p.bot);   // reuses the existing bots/ loader (http only)
   save();
   ```
   Reset the select to the `"preset…"` option afterwards. (Applying just sets already-persisted fields,
   so no new persistence is needed.)

- **Depends on** the split-wedge feature (the prototype preset uses `wedgeL`/`wedgeR`).
- **`bot` load** uses `fetch` (works when served over http / on Pages; on `file://` it just sets the
  config and skips the code — same caveat as the bot loader).

**Done when:** picking **"Prototype side wedge"** for Bot A sets `wedge L≈0.05 / R≈0.30`, loads the
`wedge-lever` bot, and sets start position 4 in one click; running a match (and a ⚔ tournament vs the
bot zoo) uses that setup.

---

## Suggested file layout / deliverables

```
index.html            the simulator (+ harness button after Phase 2)
bots/
  rammer.js           paste-ready control-loop bodies
  flanker.js
  edge-lurer.js
  juker.js
  spiral-searcher.js
  manifest.json       (if you add the loader dropdown)
firmware/
  sumobot.ino         the C port (Phase 5)
README.md  PLAN.md  RULES.md
```

Commit per phase; keep `index.html` runnable at every commit (snapshot before any big refactor).
