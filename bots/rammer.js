// rammer — baseline: edge-safe, spin-search, charge straight.
// The benchmark every other bot is measured against.

// During the freeze, sensors are live but motors are ignored. Remember the enemy.
if (frozen) {
  if (enemy.detected) memory.lastBearing = enemy.bearing;
  setMotors(0, 0);
  return;
}

// 1) SELF-PRESERVATION — never drive off the ring. Overrides everything.
if (edge.fl && edge.fr) { setMotors(-1, -1); return; }   // nose over the line -> reverse
if (edge.fl)            { setMotors( 0.6, -1); return; }  // back-right to safety
if (edge.fr)            { setMotors(-1,  0.6); return; }  // back-left to safety
if (edge.bl || edge.br) { setMotors( 1,  1); return; }    // rear over the line -> drive in

// 2) ATTACK — chase and ram.
if (enemy.detected) {
  memory.lastBearing = enemy.bearing;
  if (enemy.bearing >  12)      setMotors(1, 0.15);   // enemy right -> turn right
  else if (enemy.bearing < -12) setMotors(0.15, 1);   // enemy left  -> turn left
  else                          setMotors(1, 1);      // dead ahead -> full charge
  return;
}

// 3) SEARCH — spin toward where we last saw them.
const dir = (memory.lastBearing || 0) >= 0 ? 1 : -1;
setMotors(0.55 * dir, -0.55 * dir);
