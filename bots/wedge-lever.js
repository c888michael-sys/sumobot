// wedge-lever — the "prototype side wedge" maneuver.
// Pairs with an asymmetric chassis (low LEFT wedge): approach at a slight angle so the low
// left corner makes first contact and slips under, then lever left while pushing to swing
// onto the opponent's flank and drive them out. Phase machine; tune the thresholds.

if (frozen) { if (enemy.detected) memory.lastBearing = enemy.bearing; setMotors(0, 0); return; }

// SELF-PRESERVATION first — overrides every phase.
if (edge.fl && edge.fr) { setMotors(-1, -1); memory.phase = 'approach'; return; }
if (edge.fl)            { setMotors( 0.6, -1); memory.phase = 'approach'; return; }
if (edge.fr)            { setMotors(-1,  0.6); memory.phase = 'approach'; return; }
if (edge.bl || edge.br) { setMotors( 1,  1); return; }

memory.phase = memory.phase || 'approach';

if (enemy.detected) {
  memory.lastBearing = enemy.bearing;
  const close = enemy.distance < 14;            // ~contact range (bots are ~20 cm)

  if (memory.phase === 'approach') {
    setMotors(1, 0.78);                          // arc in, slight right — keep closing speed (arc, not pivot)
    memory.eng = close ? (memory.eng || 0) + 1 : 0;
    if (memory.eng > 8) memory.phase = 'lever';  // proxy for "low corner is under / we're engaged"

  } else if (memory.phase === 'lever') {
    setMotors(0.55, 1);                          // turn left while pushing (right faster) -> swing to flank
    if (Math.abs(enemy.bearing) > 45) memory.phase = 'push';
    if (!close) { memory.phase = 'approach'; memory.eng = 0; }

  } else { // push — drive through their exposed side
    if (enemy.bearing >  8) setMotors(1, 0.2);
    else if (enemy.bearing < -8) setMotors(0.2, 1);
    else setMotors(1, 1);
    if (!enemy.detected || enemy.distance > 22) { memory.phase = 'approach'; memory.eng = 0; }
  }
  return;
}

// SEARCH.
memory.phase = 'approach'; memory.eng = 0;
const dir = (memory.lastBearing || 0) >= 0 ? 1 : -1;
setMotors(0.55 * dir, -0.55 * dir);
