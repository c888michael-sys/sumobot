// flanker — circle to the opponent's side, then ram the exposed flank.
// Exploits the front-to-side push: a nose-on-flank hit strips the victim's grip.

if (frozen) {
  if (enemy.detected) memory.side = enemy.bearing >= 0 ? 1 : -1; // pick a side to circle
  setMotors(0, 0);
  return;
}

// SELF-PRESERVATION first.
if (edge.fl && edge.fr) { setMotors(-1, -1); return; }
if (edge.fl)            { setMotors( 0.6, -1); return; }
if (edge.fr)            { setMotors(-1,  0.6); return; }
if (edge.bl || edge.br) { setMotors( 1,  1); return; }

if (enemy.detected) {
  memory.lastBearing = enemy.bearing;
  const side = memory.side || (enemy.bearing >= 0 ? 1 : -1);
  memory.side = side;

  if (enemy.distance > 24) {
    // ARC AROUND: hold the enemy off to one side (~35°) so we approach its flank, not its nose.
    const want = 35 * side;                 // desired bearing to keep the enemy at
    if (enemy.bearing < want - 8)      setMotors(1, 0.55);   // turn right toward `want`
    else if (enemy.bearing > want + 8) setMotors(0.55, 1);   // turn left toward `want`
    else                               setMotors(1, 0.8);    // curve forward around it
  } else {
    // CLOSE: snap straight at it — we should now be beside it, so this is a side hit.
    if (enemy.bearing >  8)      setMotors(1, 0.1);
    else if (enemy.bearing < -8) setMotors(0.1, 1);
    else                         setMotors(1, 1);
  }
  return;
}

// SEARCH.
const dir = (memory.lastBearing || 0) >= 0 ? 1 : -1;
setMotors(0.55 * dir, -0.55 * dir);
