// juker — feints: charge, then jab off-axis just before contact to slip the
// opponent's wedge and arrive at its side instead of its nose.

if (frozen) { if (enemy.detected) memory.lastBearing = enemy.bearing; setMotors(0, 0); return; }

// SELF-PRESERVATION first.
if (edge.fl && edge.fr) { setMotors(-1, -1); return; }
if (edge.fl)            { setMotors( 0.6, -1); return; }
if (edge.fr)            { setMotors(-1,  0.6); return; }
if (edge.bl || edge.br) { setMotors( 1,  1); return; }

if (enemy.detected) {
  memory.lastBearing = enemy.bearing;
  const side = (memory.lastBearing || 0) >= 0 ? 1 : -1;

  // Just before contact: veer hard to one side to dodge their wedge and slide alongside.
  if (enemy.distance < 16 && Math.abs(enemy.bearing) < 30) {
    memory.juking = 14;
  }
  if (memory.juking > 0) {
    memory.juking--;
    setMotors(side > 0 ? 1 : 0.1, side > 0 ? 0.1 : 1);   // sharp veer to `side`
    return;
  }

  // Approach: aim at them at speed.
  if (enemy.bearing >  12)      setMotors(1, 0.25);
  else if (enemy.bearing < -12) setMotors(0.25, 1);
  else                          setMotors(1, 1);
  return;
}

// SEARCH.
const dir = (memory.lastBearing || 0) >= 0 ? 1 : -1;
setMotors(0.55 * dir, -0.55 * dir);
