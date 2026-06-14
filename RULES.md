# SUMOBOTS — rulebook summary

Condensed from the RAMSoc SUMOBOTS rulebook. The simulator models the items in **bold**.
This is a working summary, not the authoritative document — always check the official rulebook.

## Objective
Two robots compete head-to-head; **force your opponent out of the ring**. Fully **autonomous**
— remote control is banned.

## Streams
- **Standard** — beginner-friendly, UNSW students only.
- **Open** — advanced, open to UNSW and non-UNSW participants.

## Match format
- **Best of 3 rounds; win two rounds to win the match.**
- **Each round lasts up to 2 minutes.**
- After the judge's start call, robots must **wait 5 seconds before moving**.
- One timeout per match, up to two minutes.

## Arena
- **Circular ring, 120 cm outer / 115 cm inner diameter.**
- Black surface with a **2.5 cm white border line**.
- 18 mm MDF, spray-painted. 100 cm clearance kept around the ring (so bots don't pick up false sensor data).

## Starting position & direction (§3.3, Figure 2)
- **Four starting positions to choose from.** Each team picks one **before the match** and informs
  the judge; you **cannot change** until the next round. Robots start on **opposing sides** (§1.4.9).
- **"Forward" is whatever direction the main ultrasonic sensor / wedge ramp faces** (i.e. the forward
  defined in your code). The Figure 2 arrow marks this heading.
- *Strategic note:* choosing your start position/orientation relative to the opponent is a real lever
  (e.g. start angled to go for a flank). **The simulator does not model this yet** — see PLAN.md.

## Robot specs
| | Standard | Open |
|---|---|---|
| Max width/length | 200 mm | 250 mm |
| Height | unlimited | unlimited |
| Max weight | 1 kg | 1.5 kg |
| Budget | none | $200 ($100 on RAMSoc base kit) |
| Fuse | 1–5 A | 1–10 A |

- **Max 2 motor-driven wheels.** Standard: no drivetrain mods. Open: unrestricted drivetrain.
- Non-powered wheels allowed. 3D printing in PLA/PETG is free from budget.

## Sensors (Standard)
- Up to 4 IR sensors and up to 4 ultrasonic sensors (including kit-provided units).
- Cosmetic LEDs allowed.

## Banned
Remote control · jamming devices (e.g. IR LEDs to saturate opponent IR) · weapons ·
liquid/powder/gas storage · throwing mechanisms · traction adhesives · magnets/vacuum for
adhesion · sharp edges (radius must exceed 0.005″) · intentionally damaging the opponent.

## Winning a round
- **Force any part of the opponent out of the ring** — i.e. it touches any non-ring surface (§1.5.1.1), or
- the opponent falls out on its own, or
- **the opponent stops moving for 5+ seconds**, or
- any of the above is true at the moment the match ends.
- A robot that **falls over but is still in the ring does NOT lose** — the match continues (§1.5.2).
- *Sim note:* the simulator currently uses "centre crosses the edge" (a simplification of "any part out").

## Rematches / ties
- Entangled or orbiting with no progress for 5 s → rematch.
- Both robots stopped 5+ s → rematch.
- No winner within the time limit → tie (extra round; persistent ties → judge decision).

## Violations
- **Major** (lose the match): non-compliant robot, insulting conduct/images.
- **Minor** (cumulative; two = opponent wins the round): entering the ring during a match,
  excessive delays, starting before the 5 s mark, unsportsmanlike conduct.

## Boss battles
Teams may face a RAMSoc bossbot that can break one spec (weight/size/sensors/budget) but cannot
use damage-causing mechanics.
