// spiral-searcher — sweeps the ring with an expanding spiral for fast, complete
// coverage when it can't see the enemy, then commits to a straight ram on contact.

if (frozen) { if (enemy.detected) memory.lastBearing = enemy.bearing; setMotors(0, 0); return; }

// SELF-PRESERVATION first.
if (edge.fl && edge.fr) { setMotors(-1, -1); memory.s = 0; return; }
if (edge.fl)            { setMotors( 0.6, -1); memory.s = 0; return; }
if (edge.fr)            { setMotors(-1,  0.6); memory.s = 0; return; }
if (edge.bl || edge.br) { setMotors( 1,  1); return; }

if (enemy.detected) {
  memory.s = 0;                              // reset the spiral for next time we lose them
  memory.lastBearing = enemy.bearing;
  if (enemy.bearing >  12)      setMotors(1, 0.15);
  else if (enemy.bearing < -12) setMotors(0.15, 1);
  else                          setMotors(1, 1);
  return;
}

// SEARCH — expanding spiral: start tight, widen the arc over time so we cover the
// whole ring quickly. The inner wheel speeds up as `s` grows -> radius increases.
memory.s = (memory.s || 0) + 1;
const inner = Math.min(0.85, 0.15 + memory.s * 0.004);
setMotors(0.9, inner);
