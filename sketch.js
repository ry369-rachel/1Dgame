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
// move sound (play when selectors move) + win/lose sounds
let moveSound;
let winSound;
let loseSound;

function setup() {
  let cnv = createCanvas(canvasW, canvasH);
  cnv.parent('sketch-holder');
  rectMode(CORNER);
  textAlign(CENTER, CENTER);
  noStroke();
  generateReference();
}

function preload() {
  // load move sound; place 0001.mp3 in the project root (next to index.html)
  if (typeof loadSound === 'function') {
    soundFormats('mp3');
    moveSound = loadSound('0001.mp3', () => {}, (e) => { console.log('move sound load error', e); });
    winSound = loadSound('winning.mp3', () => {}, (e) => { console.log('win sound load error', e); });
    loseSound = loadSound('losing.mp3', () => {}, (e) => { console.log('lose sound load error', e); });
  } else {
    console.log('p5.sound not available');
  }
}

function playMoveSound() {
  try {
    if (moveSound && moveSound.isLoaded()) moveSound.play();
  } catch (e) {
    // swallow errors if sound not available
  }
}

function playWinSound() {
  try {
    if (winSound && winSound.isLoaded()) winSound.play();
  } catch (e) {}
}

function playLoseSound() {
  try {
    if (loseSound && loseSound.isLoaded()) loseSound.play();
  } catch (e) {}
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

  // center the reference block and the tiles row vertically
  // totalGroupHeight = reference block height + gap(28) + tiles row height
  let totalGroupHeight = refH + 28 + tileH;
  let topY = height / 2 - totalGroupHeight / 2;
  let refX = centerX - refW / 2;
  let refY = topY;
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
  // Add a small adjustable offset so we can move the entire row down without
  // affecting the reference block position.
  let tileRowOffset = 60; // px to push the tile row down
  let rowY = refY + refH + 28 + tileRowOffset;
  let totalRowWidth = tiles * (tileW + 2);
  let rowStartX = centerX - totalRowWidth / 2;

  // Compute live mix color: start as white, but once players move (lastActive != null)
  // show the blended color and update it as players move. If locked, keep mixColor
  // as computed at lock (computeMixAndCheck already set it), otherwise compute live.
  if (!locked) {
    if (lastActive !== null) {
      let cR = colorForIndex(redIndex);
      let cB = colorForIndex(blueIndex);
      mixColor = color(
        round((red(cR) + red(cB)) / 2),
        round((green(cR) + green(cB)) / 2),
        round((blue(cR) + blue(cB)) / 2)
      );
    } else {
      mixColor = color(255); // initial neutral white until players start moving
    }
  }

  // draw tiles
  for (let i = 0; i < tiles; i++) {
    let x = rowStartX + i * (tileW + 2);
    let y = rowY;
    // for the central mix tile, use the live mixColor so it updates as players move
    if (i === mixIndex) {
      fill(mixColor);
    } else {
      fill(colorForIndex(i));
    }
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
  // base selector Y should match the tile row so the frame overlaps the tile
  let ry = rowY;
  push();
  if (lastActive === 'red' && !locked) {
    stroke('#00FFFF');
    strokeWeight(3);
  } else {
    stroke(0);
    strokeWeight(3);
  }
  noFill();
  // draw selector frame exactly the same size as a tile and aligned to it
  rect(rx, ry, tileW, tileH);
  // label text (no colored square) — move text up closer to selector
  noStroke();
  fill(0);
  textSize(12);
  text('Player 1', rx + tileW / 2, ry + tileH + 14);
  pop();

  // Blue selector
  let bx = rowStartX + blueIndex * (tileW + 2);
  // base selector Y should match the tile row so the frame overlaps the tile
  let by = rowY;
  push();
  if (lastActive === 'blue' && !locked) {
    stroke('#00FFFF');
    strokeWeight(3);
  } else {
    stroke(0);
    strokeWeight(3);
  }
  noFill();
  // draw selector frame exactly the same size as a tile and aligned to it
  rect(bx, by, tileW, tileH);
  // label text (no colored square) — move text up closer to selector
  noStroke();
  fill(0);
  textSize(12);
  text('Player 2', bx + tileW / 2, by + tileH + 14);
  pop();

  // (Large mix preview removed) we already draw the small mix tile in the row

  // If locked and we have a message (win/lose), show it
  if (stateMessage) {
    fill(0);
    textSize(20);
    // place message near the tiles; add tileRowOffset so the popup moves down
    // when the tile row has been shifted (keeps relative spacing)
    text(stateMessage, centerX, refY + refH + 6 + (typeof tileRowOffset !== 'undefined' ? tileRowOffset : 0));
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

  // remember previous indices to detect actual movement
  let prevRed = redIndex;
  let prevBlue = blueIndex;

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

  // if either selector actually moved, play move sound
  if (redIndex !== prevRed || blueIndex !== prevBlue) {
    playMoveSound();
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
    // play win sound, keep locked and show victory, then restart after a short delay
    playWinSound();
    setTimeout(() => {
      generateReference();
    }, 1400);
  } else {
    stateMessage = 'No Match';
    // play lose sound and restart after a short delay
    playLoseSound();
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
