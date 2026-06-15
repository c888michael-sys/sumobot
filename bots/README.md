# Bot library (Phase 3)

Each `.js` file here is a **paste-ready control-loop body** — exactly the text you'd drop into the
Bot A / Bot B box in the simulator. No wrapper, no exports. Load one via the **"load into A/B…"**
dropdown under the code editors (needs the page served over http — use the `sumo-sim` launch config),
or just open the file and paste it.

All bots lead with the same **edge-guard preamble** (self-preservation overrides everything) so they
don't self-out.

## The strategies

| Bot | Idea |
|---|---|
| `rammer` | Baseline. Edge-safe, spin-search, charge straight, push. Simple and strong. |
| `spiral-searcher` | Same attack as rammer, but searches by **moving** (expanding spiral) instead of spinning in place. |
| `flanker` | Arcs toward the opponent's **side/rear** to land a front-to-side push. |
| `juker` | Charges, then jabs off-axis just before contact to slip the opponent's wedge. |
| `counter-puncher` (`edge-lurer.js`) | Patient: holds the centre, keeps its front to the enemy, only commits when close and centred. |

## Measured results — each bot vs `rammer`, identical chassis

Side-balanced harness (alternating sides), randomised start positions + heading jitter. Win % is the
**challenger's**; the rest of each row is rammer-wins + draws. Small samples — treat as indicative
(±~10 pts), and re-run with larger `n` before trusting any single number.

| Challenger vs rammer | Challenger W | rammer W | Draws | n | Verdict |
|---|---|---|---|---|---|
| `spiral-searcher` | ~22% | ~36% | ~42% | 50 | loses slightly (drawish) |
| `flanker` | ~20% | ~60% | ~20% | 20 | loses |
| `juker` | ~10% | ~30% | ~60% | 20 | loses (very drawish) |
| `counter-puncher` | ~8% | ~83% | ~8% | 24 | loses badly (too passive) |

## The finding (this is the useful part)

**At equal chassis, nothing here reliably beats a competent edge-safe rammer.** Two model
properties drive this, and both mirror real robot sumo:

1. **Never turn broadside.** With the front-to-side push, the instant you present a flank to face the
   opponent's nose, you lose. So flanking and juking — which rotate you side-on during the approach —
   are punished. Keeping your front to the enemy is the only safe orientation.
2. **Momentum beats patience.** A creeping counter-puncher gets bulldozed by a bot arriving at speed.
   Aggression wins the shove.

So a head-on game between two front-facing aggressors tends to **draw** (both stall), and the few
decisive rounds split ~evenly. Clever code doesn't break the symmetry.

**What *does* break it is chassis** — and that's config, not code. Re-run any matchup after giving one
bot a lower `wedge` or more `mass`:

```
rammer (wedge 0.5) vs rammer (wedge 0.1, mass 1.5)  ->  the low heavy wedge wins ~77% / 0 draws
```

**Takeaway for the real competition:** spend your effort on mechanical advantage (low wedge, weight,
traction) and a simple, robust rammer that **never self-outs and never stalls** — not on elaborate
strategy. The fancy bots here are kept as a teaching set showing *why* the clever moves underperform.

## Run your own tournament

1. Load a bot into A and another into B (or paste).
2. Set the chassis fields (mass / wedge / grip) — equal to test strategy, unequal to test chassis.
3. Click **⚔ Run** with a match count (start at 100). Results print to the console as win %.

Bigger `n` = less noise. Drawish matchups run slower (rounds play to the stall/time limit).
