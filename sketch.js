// Color Mix Game using p5.js
// Controls
// A / D -> move Red selector
// J / L -> move Blue selector
// R -> lock and mix

let leftCount = 7; // number of red tiles
let rightCount = 7; // number of blue tiles
let mixIndex = leftCount; // index of the central mix tile
let tiles = leftCount + 1 + rightCount; // total positions (7 red + 1 mix + 7 blue)
let tileW = 48;
let tileH = 48;
let padding = 8;
let canvasW = Math.max(tiles * (tileW + 2) + 200, 900);
let canvasH = 420;

let redIndex = 0;
let blueIndex = tiles - 1;

let lastActive = null; // 'red' or 'blue' - last moved (for the cyan stroke highlight)
let locked = false;
let refColor;
let refIndices = [0, 0];
let mixColor;
let stateMessage = '';

function setup() {
  let cnv = createCanvas(canvasW, canvasH);
  cnv.parent('sketch-holder');
  rectMode(CORNER);
  textAlign(CENTER, CENTER);
  noStroke();
  generateReference();
}

function generateReference() {
  // pick one index from the red side and one from the blue side
  refIndices[0] = floor(random(0, leftCount));
  // skip the central mix index when picking blue side
  refIndices[1] = leftCount + 1 + floor(random(0, rightCount));
  let c1 = colorForIndex(refIndices[0]);
  let c2 = colorForIndex(refIndices[1]);
  refColor = color(
    round((red(c1) + red(c2)) / 2),
    round((green(c1) + green(c2)) / 2),
    round((blue(c1) + blue(c2)) / 2)
  );
  locked = false;
  mixColor = color(255); // default white
  stateMessage = '';
  // default selectors near edges
  redIndex = 0;
  blueIndex = tiles - 1;
  lastActive = null;
}

function colorForIndex(i) {
  // returns a p5.Color. Left side (0..leftCount-1) are red shades,
  // index == mixIndex is neutral white, right side (mixIndex+1..tiles-1) are blue shades.
  if (i < leftCount) {
    // red shades from pure red to a softer pink
    let t = leftCount === 1 ? 0 : constrain(i / (leftCount - 1), 0, 1);
    let r = 255;
    let g = round(lerp(0, 150, t));
    let b = round(lerp(0, 150, t));
    return color(r, g, b);
  } else if (i === mixIndex) {
    return color(255); // neutral white for the mix tile in the row
  } else {
    // blue shades from softer light-blue to pure blue
    let j = i - (leftCount + 1);
    let t = rightCount === 1 ? 0 : constrain(j / (rightCount - 1), 0, 1);
    let r = round(lerp(120, 0, t));
    let g = round(lerp(120, 0, t));
    let b = 255;
    return color(r, g, b);
  }
}

function draw() {
  background(255);

  // draw reference color block
  let refW = 48;
  let refH = 48;
  let centerX = width / 2;
  let refX = centerX - refW / 2;
  let refY = 36;
  stroke(40);
  strokeWeight(1);
  fill(refColor);
  rect(refX, refY, refW, refH);
  noStroke();
  fill(0);
  textSize(14);
  text('REFERENCE COLOR', centerX, refY - 18);

  // (Removed large mix preview) we'll draw only the small mix tile in the row

  // Draw tiles row (visual gradient) below the reference block
  let rowY = refY + refH + 28;
  let totalRowWidth = tiles * (tileW + 2);
  let rowStartX = centerX - totalRowWidth / 2;

  // draw tiles
  for (let i = 0; i < tiles; i++) {
    let x = rowStartX + i * (tileW + 2);
    let y = rowY;
    fill(colorForIndex(i));
    stroke(0);
    strokeWeight(1);
    rect(x, y, tileW, tileH);

    // draw thin separators like picture
    stroke(0, 40);
    strokeWeight(1);
    line(x + tileW, y, x + tileW, y + tileH);
    noStroke();
  }

  // If locked, redraw the small mix tile in the row with the mixColor (same size as others)
  let mixTileX = rowStartX + mixIndex * (tileW + 2);
  if (locked) {
    stroke(0);
    strokeWeight(1);
    fill(mixColor);
    rect(mixTileX, rowY, tileW, tileH);
    noStroke();
  }

  // Draw selectors (small framed squares around the chosen tiles)
  // Red selector
  // ensure selectors are inside their allowed ranges
  redIndex = constrain(redIndex, 0, leftCount - 1);
  blueIndex = constrain(blueIndex, mixIndex + 1, tiles - 1);

  let rx = rowStartX + redIndex * (tileW + 2);
  let ry = rowY - 10;
  push();
  if (lastActive === 'red' && !locked) {
    stroke('#00FFFF');
    strokeWeight(5);
  } else {
    stroke(0);
    strokeWeight(1);
  }
  noFill();
  rect(rx - 6, ry - 6, tileW + 12, tileH + 12);
  // fill a semi-square showing player's color (left label)
  noStroke();
  fill(255, 0, 0);
  rect(rx + tileW / 2 - 10, ry + tileH + 18, 20, 20);
  fill(0);
  textSize(12);
  text('Red', rx + tileW / 2, ry + tileH + 42);
  pop();

  // Blue selector
  let bx = rowStartX + blueIndex * (tileW + 2);
  let by = rowY - 10;
  push();
  if (lastActive === 'blue' && !locked) {
    stroke('#00FFFF');
    strokeWeight(5);
  } else {
    stroke(0);
    strokeWeight(1);
  }
  noFill();
  rect(bx - 6, by - 6, tileW + 12, tileH + 12);
  noStroke();
  fill(0, 0, 255);
  rect(bx + tileW / 2 - 10, by + tileH + 18, 20, 20);
  fill(0);
  textSize(12);
  text('Blue', bx + tileW / 2, by + tileH + 42);
  pop();

  // (Large mix preview removed) we already draw the small mix tile in the row

  // If locked and we have a message (win/lose), show it
  if (stateMessage) {
    fill(0);
    textSize(20);
    // place message above the row (near the reference area)
    text(stateMessage, centerX, refY + refH + 6);
  }

  // Debug: optionally show reference indices (hidden normally) - comment out if undesired
  // textSize(12); text('ref indices: '+refIndices[0] + ','+refIndices[1], centerX, refY + refH + 2);

  // show tiny legend for which keys do what
  fill(50);
  textSize(12);
  text('A/D = move Red • J/L = move Blue • R = lock', centerX, height - 18);
}

function keyPressed() {
  if (locked) return; // ignore movement keys when locked

  // make keys case-insensitive
  let k = key.toLowerCase();

  if (k === 'a') {
    redIndex = max(0, redIndex - 1);
    lastActive = 'red';
  } else if (k === 'd') {
    // constrain red selector to left side only
    redIndex = min(leftCount - 1, redIndex + 1);
    lastActive = 'red';
  } else if (k === 'j') {
    // constrain blue selector to right side only (skip mixIndex)
    blueIndex = max(mixIndex + 1, blueIndex - 1);
    lastActive = 'blue';
  } else if (k === 'l') {
    blueIndex = min(tiles - 1, blueIndex + 1);
    lastActive = 'blue';
  } else if (k === 'r') {
    // lock and mix
    locked = true;
    computeMixAndCheck();
  }
}

function computeMixAndCheck() {
  // compute colors from each selection
  let cR = colorForIndex(redIndex);
  let cB = colorForIndex(blueIndex);
  mixColor = color(
    round((red(cR) + red(cB)) / 2),
    round((green(cR) + green(cB)) / 2),
    round((blue(cR) + blue(cB)) / 2)
  );

  // compare mixColor to refColor exactly
  if (colorsEqual(mixColor, refColor)) {
    stateMessage = 'YOU WIN!';
    // keep locked and show victory
  } else {
    stateMessage = 'No match — restarting...';
    // restart after a short delay
    setTimeout(() => {
      generateReference();
    }, 1400);
  }
}

function colorsEqual(a, b) {
  return round(red(a)) === round(red(b)) && round(green(a)) === round(green(b)) && round(blue(a)) === round(blue(b));
}

// optional: allow manual restart with space
function keyTyped() {
  if (key === ' ' ) {
    generateReference();
  }
}

// Handle window resizing gracefully
function windowResized() {
  // keep canvas fixed size for layout simplicity
}
