# 🤖 SUMOBOTS Simulator

A single-file, browser-based **autonomous robot-sumo simulator** for designing and testing
bot strategies before building hardware. Built for RAMSoc's **SUMOBOTS** competition
(two robots push each other out of a 120 cm ring; fully autonomous, no remote control).

> **Status:** the **simulator is complete and verified**. The competition **strategy bots**
> and the **hardware firmware port** are not built yet — see [PLAN.md](PLAN.md) for the build roadmap.

## Quick start

No build step, no dependencies.

- **Open locally:** double-click `index.html` (runs straight from disk in any modern browser), or
- **Serve it:** `python -m http.server 8123 -d .` then visit <http://localhost:8123>, or
- **Publish it:** enable GitHub Pages on this repo (Settings → Pages → deploy from `main` / root) and
  the simulator is live at `https://c888michael-sys.github.io/sumobot/`.

Edit the **Bot A** and **Bot B** code boxes, press **Run match**, watch the best-of-3 play out.

## What it does

- **Top-down 2D rigid-body physics.** Pushing is **traction-limited** (`force ≤ µ·load`), so mass,
  grip, and getting-under decide the shove — not raw motor power, just like real sumo.
- **Wedge under-ride model.** Each bot has a `wedge height` (0–1); the lower wedge strips the
  opponent's traction (their wheels "lift"). Lets you test **low-wedge vs high-wedge**.
- **Faithful to the rulebook:** 120 cm ring + 2.5 cm white border, 5 s start freeze, best-of-3,
  120 s rounds, win by pushing the opponent's centre past the edge **or** if it stalls > 5 s.
- **Four selectable start positions per bot** (the Figure 2 spots — pick position + heading before
  the round; shown as faint numbered markers in the arena). (See [RULES.md](RULES.md).)
- **Realistic sensing:** IR distance rays, a fused enemy bearing/distance reading, and four
  corner line-sensors for edge detection.
- Editable chassis params per bot (mass, grip µ, size, wedge height, max speed). Code + config
  auto-save to your browser's localStorage.

## Writing a bot

Your code is the control loop, run ~120 Hz. The API mirrors real firmware (**sense → decide → drive**),
so the logic ports cleanly to Arduino/C later.

```js
// don't drive yourself out
if (edge.fl || edge.fr) { setMotors(-1, -1); return; }

// chase and ram
if (enemy.detected) {
  if (enemy.bearing > 12)       setMotors(1, 0.15);   // enemy to the right -> turn right
  else if (enemy.bearing < -12) setMotors(0.15, 1);   // enemy to the left  -> turn left
  else                          setMotors(1, 1);       // dead ahead -> charge
  return;
}

// search
setMotors(0.6, -0.6);
```

| In scope | Meaning |
|---|---|
| `enemy` | `{ detected, distance /*cm*/, bearing /*deg, + = right, 0 = ahead*/ }` |
| `sensors` | array of IR rays `[{ angle, distance }]` (`distance = Infinity` if no hit) |
| `edge` | line sensors at the corners `{ fl, fr, bl, br }` — `true` when over the white border |
| `time` | seconds since the freeze ended (negative during the 5 s countdown) |
| `frozen` | `true` during the 5 s countdown (motors ignored, but sensors are live) |
| `memory` | a `{}` that persists across ticks — your robot's RAM |
| `start` | your chosen start position `1–4` (Figure 2), so code can branch on where it began |
| `setMotors(l, r)` | drive: each `-1…1` |
| `log(...)` | print to the console panel |

## Repo structure

```
index.html      the entire simulator (physics + UI + default bot)
README.md       this file
PLAN.md         build roadmap for the strategy bots and hardware port
RULES.md        SUMOBOTS rulebook summary
```

## License

MIT — do whatever you like.
