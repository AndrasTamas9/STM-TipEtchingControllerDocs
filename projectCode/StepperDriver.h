#pragma once
#include <Arduino.h>

/**
 * @brief Non-blocking stepper motor driver with position and velocity control.
 *
 * StepperDriver controls a stepper motor through STEP/DIR/ENABLE pins and
 * provides:
 *  - basic enable/disable control of the driver,
 *  - continuous velocity control in mm/s (setSpeedMmPerSec),
 *  - absolute/relative position moves in mm (moveToMm, moveRelativeMm),
 *  - a non-blocking update() method that must be called frequently from loop().
 *
 * Internally, it maintains a software position counter in steps and converts
 * between millimeters and steps based on:
 *  - stepsPerRev  (full steps per revolution),
 *  - microsteps   (microstepping factor),
 *  - lead_mm      (screw lead in mm per revolution).
 */
class StepperDriver {
public:
  /**
   * @brief Construct a new StepperDriver.
   *
   * This constructor does not move the motor but configures:
   *  - STEP, DIR, and ENABLE pins as outputs,
   *  - the steps-per-mm scaling factor,
   *  - a default speed (half of the max_mm_s parameter).
   *
   * @param stepPin      Arduino pin connected to the STEP input of the driver.
   * @param dirPin       Arduino pin connected to the DIR input of the driver.
   * @param enablePin    Arduino pin connected to the ENABLE input of the driver.
   * @param stepsPerRev  Number of full steps per motor revolution (e.g. 200).
   * @param microsteps   Microstepping factor (e.g. 16, 32).
   * @param lead_mm      Lead screw pitch (mm per revolution).
   * @param max_mm_s     Maximum allowed linear speed in mm/s.
   */
  StepperDriver(uint8_t stepPin, uint8_t dirPin, uint8_t enablePin,
                float stepsPerRev, int microsteps, float lead_mm, float max_mm_s);

  /**
   * @brief Enable or disable the stepper driver.
   *
   * This directly toggles the ENABLE pin of the driver. For a TMC2209, EN LOW
   * typically means "enabled" and EN HIGH means "disabled".
   *
   * @param on  True to enable the driver outputs, false to disable them.
   */
  void enable(bool on);

  /**
   * @brief Set continuous motion speed in mm/s.
   *
   * This configures a velocity-controlled motion mode:
   *  - v > 0: move in the positive direction,
   *  - v < 0: move in the negative direction,
   *  - v = 0: stop and switch to idle.
   *
   * The magnitude is internally limited to a safe maximum. The actual motion is
   * realized in the non-blocking update() method which must be called
   * repeatedly.
   *
   * @param v Desired linear speed in mm/s (signed).
   */
  void setSpeedMmPerSec(float v);

  /**
   * @brief Non-blocking step scheduling; call frequently from loop().
   *
   * This function:
   *  - computes the step period from the current speed,
   *  - checks whether the next step is due based on micros(),
   *  - emits one STEP pulse when needed,
   *  - automatically stops a move in Motion::ToTarget mode when the target
   *    position is reached.
   *
   * If no motion is active, it returns quickly.
   */
  void update();

  // --- Position API (mm) ---

  /**
   * @brief Set the logical position in millimeters.
   *
   * This does not move the motor; it only updates the internal step counter
   * corresponding to the given position. Useful after homing.
   *
   * @param pos_mm New position in millimeters to assign to the current state.
   */
  void  setPositionMm(float pos_mm) { pos_steps_ = (long)lroundf(pos_mm * stepsPerMm_); }

  /**
   * @brief Get the current logical position in millimeters.
   *
   * The position is derived from the internal step counter using the
   * steps-per-mm conversion factor.
   *
   * @return Current position in millimeters.
   */
  float positionMm() const          { return pos_steps_ / stepsPerMm_; }

  /**
   * @brief Get the internal conversion factor from millimeters to steps.
   *
   * @return Number of steps per millimeter.
   */
  float stepsPerMm() const          { return stepsPerMm_; }

  /**
   * @brief Get the default linear speed used when none is specified.
   *
   * This is typically half of the maximum speed given at construction.
   *
   * @return Default motion speed in mm/s.
   */
  float defaultSpeed() const        { return default_mm_s_; }

  // --- Non-blocking position moves ---

  /**
   * @brief Start a non-blocking move to an absolute position (mm).
   *
   * This sets the internal target in steps and switches the motion mode to
   * Motion::ToTarget. The actual move is executed over time by update().
   *
   * @param x_mm    Absolute target position in millimeters.
   * @param v_mm_s  Requested speed in mm/s (magnitude is used, sign comes from
   *                the direction to the target). If non-positive, default
   *                speed is used.
   */
  void moveToMm(float x_mm, float v_mm_s);

  /**
   * @brief Start a non-blocking relative move (mm).
   *
   * The target is computed as current position + dx_mm, and the driver is put
   * into Motion::ToTarget mode. Motion is realized by update().
   *
   * @param dx_mm   Relative displacement in millimeters.
   * @param v_mm_s  Requested speed in mm/s (magnitude is used, direction is
   *                derived from dx_mm). If non-positive, default speed is used.
   */
  void moveRelativeMm(float dx_mm, float v_mm_s);

  /**
   * @brief Check whether the driver is busy with a position move.
   *
   * @return true  if the motion mode is Motion::ToTarget (target not yet reached),
   * @return false otherwise (idle or velocity mode).
   */
  bool isBusy() const { return motion_ == Motion::ToTarget; }

private:
  /**
   * @brief Emit a single step pulse and update the internal position counter.
   *
   * Generates a short pulse on the STEP pin and increments or decrements
   * pos_steps_ based on the current direction flag.
   */
  void stepOnce_();

  // Control pins
  uint8_t pSTEP_, pDIR_, pEN_;

  // Configuration
  const float stepsPerMm_;
  const float default_mm_s_;

  // State
  volatile long pos_steps_ = 0; ///< Software position counter in steps.
  float  speed_mm_s_ = 0.0f;    ///< Current target speed in mm/s (signed).
  bool   dir_ = true;           ///< Direction flag: true = positive (DIR=HIGH).
  uint32_t tNextStep_ = 0;      ///< Timestamp (µs) when the next step is due.

  /**
   * @brief Motion mode indicating how update() should behave.
   *
   * - Idle     : no motion, speed is zero.
   * - Velocity : continuous motion at the configured speed.
   * - ToTarget : move until the internal target position is reached.
   */
  enum class Motion : uint8_t { Idle, Velocity, ToTarget };

  Motion motion_ = Motion::Idle; ///< Current motion mode.

  long   target_steps_ = 0;      ///< Target position in steps for ToTarget mode.
};
