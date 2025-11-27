#include "StepperDriver.h"
#include <math.h>

/**
 * @file StepperDriver.cpp
 * @brief Implementation of a non-blocking stepper driver with basic motion modes.
 *
 * The StepperDriver class controls a stepper motor through STEP/DIR/ENABLE
 * signals and provides:
 *  - continuous velocity motion (setSpeedMmPerSec),
 *  - absolute and relative target moves in millimeters (moveToMm, moveRelativeMm),
 *  - non-blocking stepping driven by periodic calls to update().
 *
 * Internally, it maintains position in steps, converts between mm and steps
 * using a configurable steps-per-revolution, microstepping factor, and screw
 * lead, and enforces a simple maximum speed limit.
 */

/**
 * @brief Construct a new StepperDriver object.
 *
 * Initializes the control pins and computes the conversion from millimeters
 * to motor steps.
 *
 * @param stepPin      Digital pin used for STEP pulses.
 * @param dirPin       Digital pin used for direction control.
 * @param enablePin    Digital pin used to enable/disable the driver.
 * @param stepsPerRev  Motor full steps per revolution (e.g. 200 for 1.8°).
 * @param microsteps   Microstepping factor (e.g. 16, 32).
 * @param lead_mm      Lead screw pitch in mm/revolution.
 * @param max_mm_s     Maximum allowed linear speed in mm/s.
 */
StepperDriver::StepperDriver(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin,
                             float stepsPerRev, int microsteps, float lead_mm, float max_mm_s)
: pSTEP_(stepPin),
  pDIR_(dirPin),
  pEN_(enablePin),
  stepsPerMm_((stepsPerRev * microsteps) / lead_mm),
  default_mm_s_(max_mm_s / 2.0) {
  pinMode(pSTEP_, OUTPUT);
  pinMode(pDIR_,  OUTPUT);
  pinMode(pEN_,   OUTPUT);
  digitalWrite(pSTEP_, LOW);
  digitalWrite(pDIR_,  LOW);
  digitalWrite(pEN_,   HIGH); // TMC2209: EN HIGH = disabled
}

/**
 * @brief Enable or disable the stepper driver.
 *
 * @param on  True to enable the driver (outputs active), false to disable it.
 */
void StepperDriver::enable(bool on) {
  digitalWrite(pEN_, on ? LOW : HIGH);   // EN LOW = enabled
}

/**
 * @brief Set continuous motion speed in mm/s.
 *
 * This method configures a velocity-based motion mode:
 *  - The magnitude is limited to ±2 * default_mm_s_.
 *  - A non-zero speed switches the motion mode to Motion::Velocity.
 *  - A zero speed switches the motion mode to Motion::Idle.
 *  - If direction changes, the DIR pin is updated accordingly.
 *  - When starting from idle, the internal step timer is reset so that
 *    stepping can begin immediately on the next update().
 *
 * @param v  Desired linear speed in mm/s (positive/negative for direction).
 */
void StepperDriver::setSpeedMmPerSec(float v) {
  float vmax = default_mm_s_ * 2.0f;
  if (v >  vmax) v =  vmax;
  if (v < -vmax) v = -vmax;

  bool wasIdle = (motion_ == Motion::Idle || speed_mm_s_ == 0.0f);

  speed_mm_s_ = v;
  motion_ = (v == 0.0f) ? Motion::Idle : Motion::Velocity;

  bool newDir = (v >= 0.0f);
  if (newDir != dir_) {
    dir_ = newDir;
    digitalWrite(pDIR_, dir_ ? HIGH : LOW);
  }

  if (v != 0.0f && wasIdle) {
    tNextStep_ = 0;   // reset only when starting from idle
  }
}

/**
 * @brief Generate a single STEP pulse and update the internal position counter.
 *
 * Produces a short pulse on the STEP pin and increments or decrements the
 * position in steps depending on the current direction flag.
 */
void StepperDriver::stepOnce_() {
  // ~2–3 µs STEP pulse
  digitalWrite(pSTEP_, HIGH);
  delayMicroseconds(2);
  digitalWrite(pSTEP_, LOW);
  pos_steps_ += dir_ ? +1 : -1;
}

/**
 * @brief Start a non-blocking move to an absolute position in millimeters.
 *
 * This method:
 *  - converts the target position from mm to steps,
 *  - sets the direction and motion mode to Motion::ToTarget,
 *  - clamps the requested speed to a safe maximum,
 *  - initializes the step timing so that stepping starts immediately.
 *
 * The actual stepping is performed in update(), which must be called
 * periodically. The motion mode automatically returns to Motion::Idle once
 * the target is reached.
 *
 * @param x_mm    Target position in millimeters.
 * @param v_mm_s  Requested linear speed in mm/s (magnitude only is used).
 *                If non-positive, default_mm_s_ is applied.
 */
void StepperDriver::moveToMm(float x_mm, float v_mm_s) {
  long tgt = (long)lroundf(x_mm * stepsPerMm_);
  target_steps_ = tgt;

  // set direction and speed
  bool goPos = (target_steps_ > pos_steps_);
  dir_ = goPos;
  digitalWrite(pDIR_, dir_ ? HIGH : LOW);

  float v = fabsf(v_mm_s);
  if (v <= 0.0f) v = default_mm_s_;
  float vmax = default_mm_s_ * 2.0f;
  if (v > vmax) v = vmax;

  speed_mm_s_ = dir_ ? +v : -v;
  motion_ = Motion::ToTarget;
  tNextStep_ = 0; // start immediately
}

/**
 * @brief Start a non-blocking relative move by the given distance in millimeters.
 *
 * The function computes a new absolute target position as current position +
 * dx_mm and delegates to moveToMm().
 *
 * @param dx_mm   Relative motion in millimeters (positive or negative).
 * @param v_mm_s  Requested speed in mm/s (magnitude only is used).
 */
void StepperDriver::moveRelativeMm(float dx_mm, float v_mm_s) {
  moveToMm(positionMm() + dx_mm, v_mm_s);
}

/**
 * @brief Periodic update function that advances the motor motion.
 *
 * This method must be called frequently (e.g. from loop()) to:
 *  - compute the required step period from the current speed,
 *  - check if it is time to produce the next STEP pulse,
 *  - perform a single step if due,
 *  - stop the motor when a Motion::ToTarget move reaches the target.
 *
 * Behavior:
 *  - If the driver is idle (no motion and zero speed), tNextStep_ is simply
 *    updated with the current time and the function returns.
 *  - If the speed is too low (< 1 step/s), no steps are produced.
 *  - Stepping is driven by a simple time comparison against tNextStep_ using
 *    micros(), taking overflow into account via signed subtraction.
 */
void StepperDriver::update() {
  // no motion
  if (motion_ == Motion::Idle && speed_mm_s_ == 0.0f) {
    tNextStep_ = micros();
    return;
  }

  // mm/s -> steps/s -> period
  float v = fabsf(speed_mm_s_);
  float stepsPerSec = v * stepsPerMm_;
  if (stepsPerSec < 1.0f) return; // too slow, do not step
  uint32_t period_us = (uint32_t)(1000000.0f / stepsPerSec);

  uint32_t now = micros();
  if (tNextStep_ == 0) tNextStep_ = now; // first start
  if ((int32_t)(now - tNextStep_) >= 0) {
    tNextStep_ = now + period_us;
    stepOnce_();

    // in target-position mode, check if we reached the target
    if (motion_ == Motion::ToTarget) {
      if (dir_) {
        // positive direction: done when pos >= target
        if (pos_steps_ >= target_steps_) {
          speed_mm_s_ = 0.0f;
          motion_ = Motion::Idle;
        }
      } else {
        // negative direction: done when pos <= target
        if (pos_steps_ <= target_steps_) {
          speed_mm_s_ = 0.0f;
          motion_ = Motion::Idle;
        }
      }
    }
  }
}
