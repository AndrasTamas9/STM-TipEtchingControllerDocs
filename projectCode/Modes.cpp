#include "Modes.h"
#include "Parameters.h" 
#include <Arduino.h>
extern float baselineCurrent;

/**
 * @brief Initialize the HOME mode and start the homing procedure.
 *
 * Behavior:
 *  - Displays a homing status message on the LCD.
 *  - Configures the limit switch pin with INPUT_PULLUP.
 *  - Enables the stepper driver and starts moving downward at a fixed speed
 *    until the limit switch is hit.
 *  - Initializes all flags and accumulators for the baseline current
 *    measurement that will be performed later at Z = 30 mm.
 */
void HomeMode::begin() {
  lcd_.title2(F("HOMING..."), F("Moving up"));
  pinMode(limitPin_, INPUT_PULLUP);
  stepper_.enable(true);
  stepper_.setSpeedMmPerSec(-5.0f);
  homed_ = false;
  baselineMeasuring_ = false;
  baselineDone_ = false;
  baselineSum_ = 0.0f;
  baselineCount_ = 0;
}

/**
 * @brief Perform one step of the HOME mode state machine.
 *
 * The homing logic proceeds through several phases:
 *  1. Move downward until the limit switch is triggered, then:
 *     - stop the motor,
 *     - set Z = 0,
 *     - move upward to Z = 30 mm.
 *  2. Once at Z = 30 mm, perform a 5 s baseline current measurement with the
 *     stepper motor stationary:
 *     - enable current measurement,
 *     - accumulate RMS current readings over the measurement period.
 *  3. After 5 s:
 *     - disable current measurement,
 *     - compute the average baseline current,
 *     - store it in the global baselineCurrent,
 *     - display the result.
 *  4. After a short delay, the mode reports completion.
 *
 * @return true  when the homing process and baseline measurement are complete,
 * @return false otherwise, meaning the mode should continue to run.
 */
bool HomeMode::step() {
  // Phase 1: homing downward
  if (!homed_) {
      stepper_.update();

      if (digitalRead(limitPin_) == LOW) {
          stepper_.setSpeedMmPerSec(0.0f);
          stepper_.setPositionMm(0.0f);   // Z = 0
          homed_ = true;

          delay(200);  // one-time small wait

          // Move upward to Z = 30 mm
          stepper_.setSpeedMmPerSec(+5.0f);
          target_ = 30.0f;

          lcd_.title2(F("HOMING"), F("Move to Z=30 mm"));
      }

      return false;
  }

  // Phase 2: moving upward to Z = 30 mm (no current measurement yet)
  stepper_.update();

  unsigned long now = millis();

  if (!baselineMeasuring_ && !baselineDone_) {
      if (stepper_.positionMm() >= target_) {
          // Stop at Z = 30 mm
          stepper_.setSpeedMmPerSec(0.0f);

          // Start the 5 s baseline measurement
          current_.setEnabled(true);
          baselineMeasuring_ = true;
          baselineStart_ = now;
          baselineSum_ = 0.0f;
          baselineCount_ = 0;

          lcd_.title2(F("HOMING"), F("Measuring I0"));
      }

      return false;
  }

  // Phase 3: at Z = 30 mm, measure RMS current for 5 seconds
  if (baselineMeasuring_) {
      // Assumes current_.update() is called in the global loop and lastIrms()
      // is kept up to date.
      float I = current_.lastIrms();

      baselineSum_   += I;
      baselineCount_ += 1;

      // If the 5-second measurement window has elapsed:
      if (now - baselineStart_ >= 5000UL) {
          current_.setEnabled(false);
          baselineMeasuring_ = false;
          baselineDone_ = true;

          float I0 = 0.0f;
          if (baselineCount_ > 0) {
              I0 = baselineSum_ / baselineCount_;
          }

          baselineCurrent = I0;   // global baseline

          lcd_.clear();
          lcd_.setCursor(0, 0);
          lcd_.print("HOME OK");

          lcd_.setCursor(0, 1);
          lcd_.print("I0=");
          lcd_.print(I0, 3);
          lcd_.print(" A");
      }

      // Stay in HOME mode while measuring
      return false;
  }

  // Phase 4: measurement finished → exit HOME mode after a short delay
  if (baselineDone_) {
      delay(2000);
      return true;   // HOME mode finished
  }

  return false;
}

/**
 * @brief Cleanup performed when leaving HOME mode.
 *
 * Currently:
 *  - Ensures the stepper driver remains enabled.
 *  - Does not change position or speed (the caller is expected to handle this).
 */
void HomeMode::end() {
  stepper_.enable(true);
}

/**
 * @brief Initialize MOD1 mode (surface detection + etching with 30V).
 *
 * Behavior:
 *  - Displays mode title on the LCD.
 *  - Configures relay pins and sets them to the initial safe state.
 *  - Resets state machine variables and current averaging helpers.
 *  - Enables the stepper motor and starts moving downward to search for the surface.
 *  - Enables current measurement for threshold-based detection and control.
 */
void Mod1Mode::begin() {
  lcd_.title2(F("MOD1: Surface detection"), F("Move down"));

  pinMode(relayPin1_, OUTPUT);
  pinMode(relayPin2_, OUTPUT);
  digitalWrite(relayPin1_, LOW);
  digitalWrite(relayPin2_, HIGH);

  st_ = State::MovingDownDetect;
  relayOn_ = false;
  pulseCount_ = 0;
  etchStart_ = 0;
  Iavg_.reset();
  IavgS_.reset();

  bumpedUp1mm_ = false; 

  stepper_.enable(true);
  stepper_.setSpeedMmPerSec(+1.5f);

  current_.setEnabled(true);
}

/**
 * @brief Execute one step of the MOD1 mode state machine.
 *
 * High-level sequence:
 *  1. Move downward until the corrected RMS current exceeds a surface threshold.
 *  2. Stop at the detected surface, then:
 *     - move a small distance further down (plunge) after a wait,
 *     - perform a short 30 V validation to confirm contact,
 *       re-entering search if contact is false.
 *  3. Once confirmed:
 *     - keep 30 V on for a 2 s pre-etch period (RelayHold),
 *     - then move upward slowly (etching) while monitoring current.
 *  4. When current falls below an etching threshold:
 *     - stop etching,
 *     - turn off 30 V,
 *     - move up by 30 mm.
 *  5. Wait for the final lift to complete, then signal the mode is done.
 *
 * Safety:
 *  - A global soft Z limit aborts the mode if the position leaves [Z_MIN, Z_MAX].
 *
 * @return true  when the mode has completed and control should return to the menu,
 * @return false while the mode is still running.
 */
bool Mod1Mode::step() {
  stepper_.update();
  unsigned long now = millis();

  // Soft limits
  const float Z_MIN = 1.5f;
  const float Z_MAX = 75.0f;

  float z = stepper_.positionMm();

  // Global safety limit: immediate abort on out-of-range Z
  if (z <= Z_MIN || z >= Z_MAX) {
    stepper_.setSpeedMmPerSec(0.0f);
    current_.setEnabled(false);
    digitalWrite(relayPin1_, HIGH);
    digitalWrite(relayPin2_, HIGH);

    lcd_.title2(F("MOD1: ABORT"), F("Z limit reached"));
    st_ = State::Done;
    return true;
  }

  // 1) Surface search using current threshold
  if (st_ == State::MovingDownDetect) {
    float Iraw = current_.correctedIrms();
    float I = IavgS_.update(Iraw);

    if (I >= threshold_) {
      stepper_.setSpeedMmPerSec(0.0f);
      stopTime_ = now;
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, HIGH);

      lcd_.title2(F("MOD1: Surface detected!"), F(""));
      lcd_.setCursor(0, 1);
      lcd_.print("I=");
      lcd_.print(I, 4);
      lcd_.print(" A   ");

      waitStart_ = now;
      st_ = State::Wait1;
    }

    return false;
  }

  // Wait1: 1 s after surface detection
  if (st_ == State::Wait1) {
    if (now - waitStart_ >= 1000UL) {
      lcd_.title2(F("MOD1: Step"), F("Down ..."));
      lcd_.setCursor(0,1);
      lcd_.print("Down ");
      lcd_.print(gParams.mod1.plungeAfterSurface_mm, 2);
      lcd_.print("mm");
      
      stepper_.moveRelativeMm(+gParams.mod1.plungeAfterSurface_mm, 1.0f);
      st_ = State::MoveDown1;
    }
    return false;
  }

  // MoveDown1: controlled plunge after surface
  if (st_ == State::MoveDown1) {
    if (!stepper_.isBusy()) {
      waitStart_ = now;
      st_ = State::Wait2;
    }
    return false;
  }

  // Wait2: 1 s before starting 30 V validation
  if (st_ == State::Wait2) {
    if (now - waitStart_ >= 1000UL) {
      // 30 V ON only for validation
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, LOW);
  
      validateStart_ = now;
      Iavg_.reset();
      IavgS_.reset();
  
      lcd_.title2(F("MOD1: Surface Test"), F("Validating..."));
      st_ = State::Validate30V;
    }
    return false;
  }

  // Validate30V: short 30 V validation pulse to confirm real contact
  if (st_ == State::Validate30V) {
    const float CONFIRM_I = 0.5f;
    const unsigned long VALIDATE_MS = 500;
  
    float Iraw = current_.correctedIrms();
    float I = IavgS_.update(Iraw);
  
    // Confirmed surface
    if (I >= CONFIRM_I) {
      lcd_.title2(F("MOD1: 30V ON"), F("Etching..."));
      etchStart_ = now;
      st_ = State::RelayHold;
      return false;
    }
  
    // False surface: turn off 30 V and resume downward search
    if (now - validateStart_ >= VALIDATE_MS) {
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, HIGH);
  
      stepper_.setSpeedMmPerSec(+3.0f);
      lcd_.title2(F("MOD1: Continue"), F("Searching..."));
      st_ = State::MovingDownDetect;
    }
  
    return false;
  }

  // RelayHold: 30 V ON, pre-etch period with current monitoring
  if (st_ == State::RelayHold) {
    float Iraw = current_.correctedIrms();
    float I = Iavg_.update(Iraw);
  
    if (now - etchStart_ < 2000UL) {
      return false;  // still in pre-etch period
    }
  
    // After pre-etch, start slow upward etching
    stepper_.setSpeedMmPerSec(-gParams.mod1.retractSpeed_mm_s);
  
    lcd_.title2(F("MOD1: Etching"), F("Rising..."));
    st_ = State::Etching;
    return false;
  }

  // Etching: 30 V ON, slow upward motion while monitoring current
  if (st_ == State::Etching) {
    float Iraw = current_.correctedIrms();
    float I = Iavg_.update(Iraw);
  
    // When current drops below the etching threshold, stop etching and lift
    if (I < gParams.mod1.etchingThreshold_A) {
      stepper_.setSpeedMmPerSec(0.0f);
  
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, HIGH);
  
      stepper_.moveRelativeMm(-30.0f, 3.0f);
      st_ = State::FinalLift;
      return false;
    }
  
    // Otherwise continue etching
    return false;
  }

  // FinalLift: wait until the final 30 mm lift finishes, then complete the mode
  if (st_ == State::FinalLift) {
    if (!stepper_.isBusy()) {
      current_.setEnabled(false);
      lcd_.title2(F("MOD1: DONE"), F(""));
      st_ = State::Done;
      return true;
    }
    return false;
  } 

  // Fallback safety: consider mode done only when state is Done
  return (st_ == State::Done);
}

/**
 * @brief Cleanup for MOD1 mode.
 *
 * Ensures:
 *  - stepper speed is set to zero and driver is enabled,
 *  - current measurement is disabled,
 *  - all relays are turned off (safe state).
 */
void Mod1Mode::end() {
  stepper_.setSpeedMmPerSec(0.0f);
  stepper_.enable(true);

  current_.setEnabled(false);

  digitalWrite(relayPin1_, HIGH);
  digitalWrite(relayPin2_, HIGH);
}

/**
 * @brief Initialize MOD2 mode (surface detection + validation + pulsed 9 V).
 *
 * Behavior:
 *  - Displays initial mode information on the LCD.
 *  - Configures relay pins and turns everything off.
 *  - Sets up state machine variables and enables the stepper motor.
 *  - Begins moving downward to detect the surface using current thresholding.
 *  - Enables current measurement for detection and validation phases.
 */
void Mod2Mode::begin() {
  lcd_.title2(F("MOD2: Surface detection"), F("Move down..."));

  pinMode(relayPin1_, OUTPUT);
  pinMode(relayPin2_, OUTPUT);
  digitalWrite(relayPin1_, HIGH);
  digitalWrite(relayPin2_, HIGH);

  st_ = State::MovingDownDetect;
  relayOn_ = false;
  pulseCount_ = 0;
  etchStart_ = 0;

  stepper_.enable(true);
  stepper_.setSpeedMmPerSec(+3.0f);

  current_.setEnabled(true);
}

/**
 * @brief Execute one step of the MOD2 mode state machine.
 *
 * High-level sequence:
 *  1. Move downward until the corrected RMS current exceeds a threshold
 *     (surface detection).
 *  2. After a short wait, move down further (plunge), wait again, then:
 *     - turn 30 V on for validation,
 *     - confirm or reject surface contact based on current.
 *  3. Once validated:
 *     - hold 30 V on (RelayHold) while monitoring current,
 *     - turn 30 V off and log current when a condition is met.
 *  4. After another wait, move down again, wait, then:
 *     - disable current measurement,
 *     - apply a series of 9 V pulses (RelayPulse) according to configured
 *       parameters,
 *     - finally lift by 30 mm and finish.
 *
 * Safety:
 *  - A global Z limit aborts the mode immediately if exceeded.
 *
 * @return true  when the mode has fully completed,
 * @return false while the mode is still in progress.
 */
bool Mod2Mode::step() {
  stepper_.update();
  unsigned long now = millis();

  // Soft limits
  const float Z_MIN = 1.5f;
  const float Z_MAX = 75.0f;

  float z = stepper_.positionMm();

  // Global safety limit: immediate abort if Z is out of bounds
  if (z <= Z_MIN || z >= Z_MAX) {
    stepper_.setSpeedMmPerSec(0.0f);
    current_.setEnabled(false);
    digitalWrite(relayPin1_, HIGH);
    digitalWrite(relayPin2_, HIGH);

    lcd_.title2(F("MOD2: ABORT"), F("Z limit reached"));
    st_ = State::Done;
    return true;
  }

  // 1) Surface search using current threshold
  if (st_ == State::MovingDownDetect) {
    float I = current_.correctedIrms();

    if (I >= threshold_) {
      stepper_.setSpeedMmPerSec(0.0f);
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, HIGH);

      lcd_.title2(F("MOD2: Surface detected!"), F(""));
      lcd_.setCursor(0, 1);
      lcd_.print("I=");
      lcd_.print(I, 4);
      lcd_.print(" A   ");

      waitStart_ = now;
      st_ = State::Wait1;
    }
    return false;
  }

  // Wait1: delay after surface detection
  if (st_ == State::Wait1) {
    if (now - waitStart_ >= 1000UL) {
      lcd_.title2(F("MOD2: Step"), F("Down ..."));
      lcd_.setCursor(0,1);
      lcd_.print("Down ");
      lcd_.print(gParams.mod2.plungeAfterSurface_mm, 2);
      lcd_.print("mm");
      
      stepper_.moveRelativeMm(+gParams.mod2.plungeAfterSurface_mm, 1.0f);
      st_ = State::MoveDown1;
    }
    return false;
  }

  // MoveDown1: first additional downward motion
  if (st_ == State::MoveDown1) {
    if (!stepper_.isBusy()) {
      waitStart_ = now;
      st_ = State::Wait2;
    }
    return false;
  }

  // Wait2: pause before 30 V validation
  if (st_ == State::Wait2) {
    if (now - waitStart_ >= 1000UL) {
      // 30 V ON only for validation
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, LOW);
  
      validateStart_ = now;
      Iavg_.reset();
      IavgS_.reset();
  
      lcd_.title2(F("MOD2: Surface Test"), F("Validating..."));
      st_ = State::Validate30V;
    }
    return false;
  }

  // Validate30V: validate surface with short 30 V pulse
  if (st_ == State::Validate30V) {
    const float CONFIRM_I = 0.5f;
    const unsigned long VALIDATE_MS = 500;
  
    float Iraw = current_.correctedIrms();
    float I = IavgS_.update(Iraw);
  
    // Confirmed surface
    if (I >= CONFIRM_I) {
      lcd_.title2(F("MOD2: 30V ON"), F("Etching..."));
      etchStart_ = now;
      st_ = State::RelayHold;
      return false;
    }
  
    // False surface: turn 30 V off and resume downward search
    if (now - validateStart_ >= VALIDATE_MS) {
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, HIGH);
  
      stepper_.setSpeedMmPerSec(+3.0f);
      lcd_.title2(F("MOD2: Continue"), F("Searching..."));
      st_ = State::MovingDownDetect;
    }
  
    return false;
  }

  // RelayHold: 30 V ON, hold position and monitor current
  if (st_ == State::RelayHold) {
    float Iraw = current_.correctedIrms();
    float I = Iavg_.update(Iraw);

    // Optional pre-etch period of 2 s with 30 V ON
    if (now - etchStart_ < 2000UL) {
      return false;
    }

    // Condition to switch 30 V OFF and proceed
    if (I <= I <= gParams.mod2.etchingThreshold_A) {
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, HIGH);

      lcd_.title2(F("MOD2: 30V OFF"), F(""));
      lcd_.setCursor(0, 1);
      lcd_.print("I=");
      lcd_.print(I, 4);
      lcd_.print(" A   ");

      waitStart_ = now;
      st_ = State::Wait3;
    }
    return false;
  }

  // Wait3: pause after 30 V OFF
  if (st_ == State::Wait3) {
    if (now - waitStart_ >= 1000UL) {
      lcd_.title2(F("MOD2: Step"), F("Down ..."));
      lcd_.setCursor(0,1);
      lcd_.print("Down ");
      lcd_.print(gParams.mod2.plungeAfterEtch_mm, 2);
      lcd_.print("mm");
      
      stepper_.moveRelativeMm(+gParams.mod2.plungeAfterEtch_mm, 1.0f);
      st_ = State::MoveDown2;
    }
    return false;
  }

  // MoveDown2: second downward move after etching phase
  if (st_ == State::MoveDown2) {
    if (!stepper_.isBusy()) {
      waitStart_ = now;
      st_ = State::Wait4;
    }
    return false;
  }

  // Wait4: final pause before pulsed 9 V sequence
  if (st_ == State::Wait4) {
    if (now - waitStart_ >= 1000UL) {
      current_.setEnabled(false);

      lcd_.title2(F("MOD2: 9V ON"), F("Pulses..."));
      pulseStart_ = now;
      relayOn_ = true;
      pulseCount_ = 0;

      // 9 V ON (mapping: relay1 LOW, relay2 HIGH)
      digitalWrite(relayPin1_, LOW);
      digitalWrite(relayPin2_, HIGH);

      st_ = State::RelayPulse;
    }
    return false;
  }

  // RelayPulse: apply a series of 9 V ON/OFF pulses
  if (st_ == State::RelayPulse) {
    if (relayOn_) {
      // ON phase
      if (now - pulseStart_ >= gParams.mod2.pulseOn_s) {
        digitalWrite(relayPin1_, HIGH);
        digitalWrite(relayPin2_, HIGH);
        relayOn_ = false;
        pulseStart_ = now;
      }
    } else {
      // OFF phase
      if (now - pulseStart_ >= gParams.mod2.pulseOff_s) {
        pulseCount_++;
        if (pulseCount_ >= gParams.mod2.pulseCount) {
          // Pulses finished → move up by 30 mm
          lcd_.title2(F("MOD2: DONE"), F(""));
          stepper_.moveRelativeMm(-30.0f, 3.0f);
          st_ = State::FinalLift;
          return false;
        } else {
          // Next pulse: 9 V ON again
          digitalWrite(relayPin1_, LOW);
          digitalWrite(relayPin2_, HIGH);
          relayOn_ = true;
          pulseStart_ = now;
        }
      }
    }
    return false;
  }

  // FinalLift: wait for the final 30 mm lift to complete
  if (st_ == State::FinalLift) {
    if (!stepper_.isBusy()) {
      digitalWrite(relayPin1_, HIGH);
      digitalWrite(relayPin2_, HIGH);
      st_ = State::Done;
      return true;
    }
    return false;
  }

  // Fallback safety
  return (st_ == State::Done);
}

/**
 * @brief Cleanup for MOD2 mode.
 *
 * Ensures:
 *  - the stepper motor is stopped and enabled,
 *  - current measurement is disabled,
 *  - all relays are turned off (safe state).
 */
void Mod2Mode::end() {
  stepper_.setSpeedMmPerSec(0.0f);
  stepper_.enable(true);

  current_.setEnabled(false);
  digitalWrite(relayPin1_, HIGH);
  digitalWrite(relayPin2_, HIGH);
}

/**
 * @brief Initialize JogMode for manual up/down positioning.
 *
 * Behavior:
 *  - Resets the local UI tick counter and enables the stepper driver.
 *  - Displays a hint on the LCD that UP/DOWN buttons control motion and SELECT
 *    exits the mode.
 *  - Marks the first step so that a spurious SELECT used to enter the mode can
 *    be cleared.
 */
void JogMode::begin(){ 
  uiTick_=0; stepper_.enable(true); 
  lcd_.title2(F("JOG (UP/DOWN)"),F("  "));
  firstStep_ = true;
}

/**
 * @brief Execute one step of the JogMode.
 *
 * Behavior:
 *  - Reads the stable key state from the keypad.
 *  - On the very first step, ignores a SELECT that might have been used to
 *    enter the mode (to avoid immediate exit).
 *  - Computes the current Z position and applies motion limits [Z_MIN, Z_MAX].
 *  - If UP is pressed and within limits, moves upward (Z decreases).
 *  - If DOWN is pressed and within limits, moves downward (Z increases).
 *  - Otherwise, stops the motor.
 *  - Periodically (every ~200 ms) updates the LCD with the current position.
 *  - Exits the mode when SELECT is pressed.
 *
 * @return true  if SELECT is pressed (mode should exit),
 * @return false otherwise.
 */
bool JogMode::step(){
  Key s = keys_.stable();

  if (firstStep_) {
    if (s == Key::SELECT) {
      keys_.clear();
      s = Key::NONE;
    }
    firstStep_ = false;
  }

  float z = stepper_.positionMm();

  // Motion limits
  const float Z_MIN = 1.5f;
  const float Z_MAX = 75.0f;

  if (s == Key::UP) {          // up button → Z decreases
    if (z > Z_MIN) {
      stepper_.setSpeedMmPerSec(-2.0f);
    } else {
      stepper_.setSpeedMmPerSec(0.0f);
    }
  }
  else if (s == Key::DOWN) {   // down button → Z increases
    if (z < Z_MAX) {
      stepper_.setSpeedMmPerSec(+2.0f);
    } else {
      stepper_.setSpeedMmPerSec(0.0f);
    }
  }
  else {
    stepper_.setSpeedMmPerSec(0.0f);
  }

  // Run the motor
  stepper_.update();

  // Periodic LCD update with current position
  if (millis() - uiTick_ >= 200) {
    uiTick_ = millis();
    lcd_.setCursor(0,1);
    lcd_.print("X=");
    lcd_.print(stepper_.positionMm(), 2);
    lcd_.print(" mm ");
  }

  // SELECT terminates the mode
  return (s == Key::SELECT);
}

/**
 * @brief Cleanup for JogMode.
 *
 * Ensures the stepper motor is stopped and enabled when leaving the mode.
 */
void JogMode::end(){ stepper_.setSpeedMmPerSec(0); stepper_.enable(true);}
