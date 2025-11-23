/*
 * FINAL Optimized Robot Eye Expression Sketch
 * Target: Arduino Uno / Nano
 *
 * This sketch is a complete refactor to solve the flash memory (size) issue.
 *
 * CHANGES:
 * 1.  REDUCED ANIMATIONS: Cut from 35 to 15 core expressions to save space.
 * 2.  NON-BLOCKING: Removed all `delay()` and blocking `for` loops from animations.
 * 3.  SENSOR-READY: The main `loop()` is now fast and non-blocking. You can
 * add sensor checks (e.g., `analogRead()`) directly in the loop.
 * 4.  EASY API: You can now set an expression by name: `setExpression(HAPPY);`
 * 5.  STATE MACHINE: Uses an `enum` (Expression) to manage the robot's state.
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -- Eye Definition --
const int16_t REF_EYE_W = 40;
const int16_t REF_EYE_H = 40;
const int16_t REF_SPACE = 10;
const int16_t CENTER_X_LEFT = SCREEN_WIDTH / 2 - REF_EYE_W / 2 - REF_SPACE / 2;
const int16_t CENTER_X_RIGHT = SCREEN_WIDTH / 2 + REF_EYE_W / 2 + REF_SPACE / 2;
const int16_t CENTER_Y = SCREEN_HEIGHT / 2;

// -- Reusable Eye State Structure --
// We use int16_t for most values for speed and size.
struct EyeState {
  int16_t x, y;      // center position
  int16_t w, h;      // width/height
  int16_t pupil_dx;  // pupil offset x
  int16_t pupil_dy;  // pupil offset y
  float   lid;       // 0.0 (open) to 1.0 (closed)
  float   intensity; // 0.0 to 1.0
};

// Current *rendered* state vs. *target* state
EyeState leftEye, rightEye;
EyeState leftTarget, rightTarget;
EyeState leftStart, rightStart; // For interpolation

// -- Core API: Expression Enum --
// This is the "easy to use" part.
// Instead of numbers, we use names.
enum Expression {
  IDLE,
  HAPPY,
  SAD,
  ANGRY,
  SURPRISED,
  BLINK,
  LOOK_LEFT,
  LOOK_RIGHT,
  CONFUSED,
  BORED,
  SCARED,
  SLEEPY,
  ASLEEP,
  WAKEUP
  // 14 expressions. Add more if space allows.
};
Expression currentExpression = WAKEUP;
Expression previousExpression = IDLE;

// -- Animation Timer --
// These control the non-blocking transitions
unsigned long transitionStartTime = 0;
unsigned long transitionDuration = 200; // 200ms default
bool inTransition = false;
bool autoBlink = true; // Set to false if you want to control blinks
unsigned long lastBlinkTime = 0;
unsigned long nextBlinkDelay = 3000; // Blink every 3-7 seconds

// -- Demo Mode Timer --
bool demo_mode = true;
unsigned long nextDemoTime = 0;
int demo_index = 0;

// -- Interpolation (Lerp) Helpers --
// These make the transitions smooth
float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}
int16_t lerp_i(int16_t a, int16_t b, float t) {
  return (int16_t)(a + (float)(b - a) * t);
}

// === HELPER FUNCTION ===
// (This was missing, causing the error)
// helper: set a state's center/size from inputs
void setEyeState(EyeState &s, int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t pdx, int16_t pdy, float lid, float intensity) {
  s.x = cx; s.y = cy; s.w = w; s.h = h; s.pupil_dx = pdx; s.pupil_dy = pdy; s.lid = lid; s.intensity = intensity;
}

// === OPTIMIZED drawEye ===
// Uses efficient `fillRoundRect`
void drawEye(const EyeState &s) {
  int16_t h = (int16_t)(s.h * (1.0 - constrain(s.lid, 0.0, 1.0)));
  if (h < 2) h = 2;
  int16_t w = s.w;
  int16_t x = s.x - w / 2;
  int16_t y = s.y - h / 2;
  int16_t radius = h / 2;
  display.fillRoundRect(x, y, w, h, radius, SSD1306_WHITE);

  int16_t rx = s.w / 2;
  int16_t visible_ry = h / 2;
  int16_t pupilMax = (int16_t)(min(rx, visible_ry) * 0.45);
  if (pupilMax < 1) pupilMax = 1;
  int16_t pdx = constrain(s.pupil_dx, -pupilMax, pupilMax);
  int16_t pdy = constrain(s.pupil_dy, -pupilMax, pupilMax);
  int16_t pupilX = s.x + pdx;
  int16_t pupilY = s.y + pdy;
  int16_t pupilR = max(1, (int16_t)(pupilMax * (0.9 - 0.3 * s.intensity)));
  display.fillCircle(pupilX, pupilY, pupilR, SSD1306_BLACK);

  int16_t glintR = max(1, pupilR / 3);
  display.fillCircle(pupilX - pupilR / 2, pupilY - pupilR / 2, glintR, SSD1306_WHITE);
}

// Clear & draw both eyes (without display.display())
void drawEyes() {
  display.clearDisplay();
  drawEye(leftEye);
  drawEye(rightEye);
}

// === MAIN EXPRESSION API ===
/*
 * This is the new, reusable function.
 * It sets the *target* state and an animation duration.
 * The `updateEyes()` function in the main loop will do the actual animation.
 */
void setExpression(Expression exp, int duration = 200) {
  // Don't interrupt a transition unless it's a new expression
  if (inTransition && exp == currentExpression) return;
  
  // Store the state *before* the transition
  leftStart = leftEye;
  rightStart = rightEye;
  
  // Set transition timers
  transitionStartTime = millis();
  transitionDuration = duration;
  inTransition = true;
  
  // If we are blinking, store the expression we were in
  if (exp != BLINK) {
    previousExpression = exp;
  }
  currentExpression = exp;

  // --- Define the 15 Target Expressions ---
  // This switch statement is MUCH smaller than 35 functions.
  switch (exp) {
    case IDLE:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y, REF_EYE_W, REF_EYE_H, 0, 0, 0.0, 0.5);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y, REF_EYE_W, REF_EYE_H, 0, 0, 0.0, 0.5);
      break;

    case HAPPY:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y - 4, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.75), 0, -3, 0.12, 0.8);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y - 4, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.75), 0, -3, 0.12, 0.8);
      break;

    case SAD:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y + 4, (int16_t)(REF_EYE_W * 0.92), (int16_t)(REF_EYE_H * 0.7), 0, 3, 0.2, 0.6);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y + 4, (int16_t)(REF_EYE_W * 0.92), (int16_t)(REF_EYE_H * 0.7), 0, 3, 0.2, 0.6);
      break;

    case ANGRY:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y, (int16_t)(REF_EYE_W * 0.85), (int16_t)(REF_EYE_H * 0.65), 4, 0, 0.25, 1.0);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y, (int16_t)(REF_EYE_W * 0.85), (int16_t)(REF_EYE_H * 0.65), -4, 0, 0.25, 1.0);
      break;

    case SURPRISED:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y, (int16_t)(REF_EYE_W * 1.3), (int16_t)(REF_EYE_H * 1.3), 0, 0, 0.0, 0.2);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y, (int16_t)(REF_EYE_W * 1.3), (int16_t)(REF_EYE_H * 1.3), 0, 0, 0.0, 0.2);
      break;

    case BLINK:
      // Just set the lid target. The `updateEyes` function will handle returning.
      leftTarget.lid = 1.0;
      rightTarget.lid = 1.0;
      transitionDuration = 75; // Fast blink
      break;

    case LOOK_LEFT:
      setEyeState(leftTarget, CENTER_X_LEFT - 8, CENTER_Y, (int16_t)(REF_EYE_W * 1.1), (int16_t)(REF_EYE_H * 1.1), -6, 0, 0.0, 0.7);
      setEyeState(rightTarget, CENTER_X_RIGHT - 8, CENTER_Y, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.9), -3, 0, 0.0, 0.6);
      break;

    case LOOK_RIGHT:
      setEyeState(leftTarget, CENTER_X_LEFT + 8, CENTER_Y, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.9), 3, 0, 0.0, 0.6);
      setEyeState(rightTarget, CENTER_X_RIGHT + 8, CENTER_Y, (int16_t)(REF_EYE_W * 1.1), (int16_t)(REF_EYE_H * 1.1), 6, 0, 0.0, 0.7);
      break;
      
    case CONFUSED:
      setEyeState(leftTarget, CENTER_X_LEFT - 4, CENTER_Y, (int16_t)(REF_EYE_W * 0.95), (int16_t)(REF_EYE_H * 0.95), 4, -2, 0.08, 0.7);
      setEyeState(rightTarget, CENTER_X_RIGHT + 4, CENTER_Y, (int16_t)(REF_EYE_W * 0.95), (int16_t)(REF_EYE_H * 0.95), -4, 2, 0.18, 0.7);
      break;

    case BORED:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y + 2, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.6), 3, 3, 0.45, 0.4);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y + 2, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.6), 3, 3, 0.45, 0.4);
      break;

    case SCARED:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y, (int16_t)(REF_EYE_W * 1.5), (int16_t)(REF_EYE_H * 1.5), 0, 0, 0.0, 0.1);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y, (int16_t)(REF_EYE_W * 1.5), (int16_t)(REF_EYE_H * 1.5), 0, 0, 0.0, 0.1);
      break;

    case SLEEPY:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y + 2, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.6), 0, 1, 0.6, 0.3);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y + 2, (int16_t)(REF_EYE_W * 0.9), (int16_t)(REF_EYE_H * 0.6), 0, 1, 0.6, 0.3);
      break;

    case ASLEEP:
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y + 4, (int16_t)(REF_EYE_W * 0.8), (int16_t)(REF_EYE_H * 0.4), 0, 0, 1.0, 0.1);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y + 4, (int16_t)(REF_EYE_W * 0.8), (int16_t)(REF_EYE_H * 0.4), 0, 0, 1.0, 0.1);
      break;
      
    case WAKEUP:
      // Start from a closed position (set in setup)
      setEyeState(leftTarget, CENTER_X_LEFT, CENTER_Y, REF_EYE_W, REF_EYE_H, 0, 0, 0.0, 0.6);
      setEyeState(rightTarget, CENTER_X_RIGHT, CENTER_Y, REF_EYE_W, REF_EYE_H, 0, 0, 0.0, 0.6);
      transitionDuration = 500; // Slow wakeup
      break;
  }
}

// === NON-BLOCKING ANIMATION ENGINE ===
/*
 * This function is called on *every* loop.
 * It calculates one frame of animation.
 * This is what makes the code "sensor-friendly".
 */
void updateEyes() {
  unsigned long now = millis();
  
  // 1. Check if a transition is in progress
  if (inTransition) {
    unsigned long elapsed = now - transitionStartTime;
    
    // 2. Is the transition over?
    if (elapsed >= transitionDuration) {
      inTransition = false;
      leftEye = leftTarget;
      rightEye = rightTarget;
      
      // 3. Handle automatic "sequences"
      if (currentExpression == BLINK) {
        // Blink is over, return to previous expression
        setExpression(previousExpression, 75);
      } else if (currentExpression == WAKEUP) {
        // Wakeup is over, go to IDLE
        setExpression(IDLE, 200);
      } else if (currentExpression == SLEEPY) {
        // Sleepy is over, go to ASLEEP
        setExpression(ASLEEP, 500);
      }
      
    } else {
      // 4. Transition is in progress, so interpolate (lerp)
      float t = (float)elapsed / (float)transitionDuration;
      
      leftEye.x        = lerp_i(leftStart.x, leftTarget.x, t);
      leftEye.y        = lerp_i(leftStart.y, leftTarget.y, t);
      leftEye.w        = lerp_i(leftStart.w, leftTarget.w, t);
      leftEye.h        = lerp_i(leftStart.h, leftTarget.h, t);
      leftEye.pupil_dx = lerp_i(leftStart.pupil_dx, leftTarget.pupil_dx, t);
      leftEye.pupil_dy = lerp_i(leftStart.pupil_dy, leftTarget.pupil_dy, t);
      leftEye.lid      = lerp(leftStart.lid, leftTarget.lid, t);
      leftEye.intensity= lerp(leftStart.intensity, leftTarget.intensity, t);

      rightEye.x        = lerp_i(rightStart.x, rightTarget.x, t);
      rightEye.y        = lerp_i(rightStart.y, rightTarget.y, t);
      rightEye.w        = lerp_i(rightStart.w, rightTarget.w, t);
      rightEye.h        = lerp_i(rightStart.h, rightTarget.h, t);
      rightEye.pupil_dx = lerp_i(rightStart.pupil_dx, rightTarget.pupil_dx, t);
      rightEye.pupil_dy = lerp_i(rightStart.pupil_dy, rightTarget.pupil_dy, t);
      rightEye.lid      = lerp(rightStart.lid, rightTarget.lid, t);
      rightEye.intensity= lerp(rightStart.intensity, rightTarget.intensity, t);
    }
  }
  
  // 5. Handle automatic blinking
  if (autoBlink && !inTransition && now > lastBlinkTime + nextBlinkDelay) {
    if (currentExpression != ASLEEP && currentExpression != BLINK) {
      setExpression(BLINK, 75);
    }
    lastBlinkTime = now;
    nextBlinkDelay = random(3000, 7000); // Blink again in 3-7s
  }
  
  // 6. Draw the current eye state
  drawEyes();
}

// === Serial Handler ===
// Easy to use: send a number (0-13) over serial to change expression
void handleSerial() {
  if (Serial.available()) {
    // Using parseInt() is smaller than parsing a String (like "A5")
    int anim = Serial.parseInt();
    
    if (anim >= 0 && anim < 14) { // Check it's a valid number
      demo_mode = false; // Receiving a command stops the demo
      autoBlink = true;
      setExpression((Expression)anim, 200);
    }
    
    // Clear the serial buffer
    while(Serial.available()) { Serial.read(); }
  }
}

// === Demo Mode Handler ===
void handleDemo() {
  if (demo_mode && !inTransition && millis() > nextDemoTime) {
    // Cycle through expressions 0 to 11 (skip ASLEEP/WAKEUP)
    demo_index = (demo_index + 1) % 12;
    setExpression((Expression)demo_index, 400); // 400ms transition
    nextDemoTime = millis() + 2000; // Wait 2 seconds
  }
}

// === SETUP ===
void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  Serial.begin(9600); // 9600 is often more reliable and uses less power

  // We must set the *initial* state, not just the target
  setEyeState(leftEye, CENTER_X_LEFT, CENTER_Y + 4, (int16_t)(REF_EYE_W * 0.8), (int16_t)(REF_EYE_H * 0.4), 0, 0, 1.0, 0.1);
  setEyeState(rightEye, CENTER_X_RIGHT, CENTER_Y + 4, (int16_t)(REF_EYE_W * 0.8), (int16_t)(REF_EYE_H * 0.4), 0, 0, 1.0, 0.1);
  leftTarget = leftEye;
  rightTarget = rightEye;
  
  // Start the wakeup sequence
  setExpression(WAKEUP, 500);
  nextDemoTime = millis() + 3000; // Start demo after wakeup
}

// === MAIN LOOP ===
/*
 * This is your new loop. It's very fast.
 * You can add your sensor code here.
 *
 * Example:
 * int sensorValue = analogRead(A0);
 * if (sensorValue > 500) {
 * setExpression(ANGRY, 100);
 * }
 */
void loop() {
  // 1. Check for inputs
  handleSerial();
  
  // 2. Handle autonomous behaviors
  handleDemo();

  // 3. Update and draw the animation (this does all the work)
  updateEyes();
  
  // 4. Send the completed frame to the screen
  display.display();
}

