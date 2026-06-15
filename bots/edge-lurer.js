// counter-puncher — patient. Holds the centre, keeps its FRONT to the enemy at all
// times (turning broadside is fatal under the front-to-side push), and only commits to
// a full-power ram when the enemy is close and dead ahead. Lets over-eager chargers
// come to it. Conservative: trades wins for never feeding its flank.

if (frozen) { if (enemy.detected) memory.lastBearing = enemy.bearing; setMotors(0, 0); return; }

// SELF-PRESERVATION first.
if (edge.fl && edge.fr) { setMotors(-1, -1); return; }
if (edge.fl)            { setMotors( 0.6, -1); return; }
if (edge.fr)            { setMotors(-1,  0.6); return; }
if (edge.bl || edge.br) { setMotors( 1,  1); return; }

if (enemy.detected) {
  memory.lastBearing = enemy.bearing;
  if (Math.abs(enemy.bearing) > 10) {
    // Re-aim with a near-stationary pivot — keep the nose on them, don't expose a side.
    if (enemy.bearing > 0) setMotors(0.5, -0.2);
    else                   setMotors(-0.2, 0.5);
  } else if (enemy.distance < 20) {
    setMotors(1, 1);                 // close + centred -> commit, nose-to-nose
  } else {
    setMotors(0.3, 0.3);             // creep forward, staying square to them
  }
  return;
}

// SEARCH — gentle, stay off the rim.
const dir = (memory.lastBearing || 0) >= 0 ? 1 : -1;
setMotors(0.45 * dir, -0.45 * dir);
