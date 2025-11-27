#pragma once
#include "IMode.h"
#include "Lcd1602.h"
#include "KeypadShield.h"
#include "StepperDriver.h"
#include "CurrentSensor.h"
#include "MovingAverage.h"

/**
 * @brief Moving average type for long-window current averaging.
 *
 * Template parameters:
 *  - 200: number of samples in the window,
 *  - 1000: nominal sampling frequency or scaling factor (implementation-specific).
 */
using IAvg_t = MovingAverage<200, 1000>;

/**
 * @brief Moving average type for short-window current averaging / smoothing.
 *
 * Template parameters:
 *  - 20:   number of samples in the window,
 *  - 1000: nominal sampling frequency or scaling factor (implementation-specific).
 */
using IAvg_s = MovingAverage<20, 1000>;

/**
 * @brief Global baseline RMS current determined during HOME mode.
 *
 * This value is typically measured when the system is idle (e.g. at Z = 30 mm)
 * and used to compensate for sensor offsets in CurrentSensor::correctedIrms().
 */
extern float baselineCurrent;

/**
 * @brief Homing mode for the Z axis with baseline current measurement.
 *
 * Responsibilities:
 *  - Perform a homing motion by driving the stepper towards a limit switch,
 *    then zeroing the position.
 *  - Move to a predefined target height (e.g. Z = 30 mm).
 *  - At that position, perform a multi-second RMS current measurement using
 *    CurrentSensor in order to establish a baseline (no-load) current.
 *  - Store the result in the global baselineCurrent variable.
 */
class HomeMode : public IMode {
public:
  /**
   * @brief Construct a new HomeMode instance.
   *
   * @param lcd        Reference to the LCD helper used for user feedback.
   * @param stepper    Reference to the stepper driver controlling the Z axis.
   * @param limitPin   Digital input pin connected to the homing limit switch.
   * @param current    Reference to the current sensor used to measure baseline.
   */
  HomeMode(Lcd1602& lcd, StepperDriver& stepper, uint8_t limitPin, CurrentSensor& current)
    : lcd_(lcd), stepper_(stepper), limitPin_(limitPin), current_(current) {}

  /**
   * @brief Get the name of this mode.
   *
   * @return Constant C-string "HOME".
   */
  const char* name() const override { return "HOME"; }

  /**
   * @brief Initialize the HOME mode.
   *
   * Sets up LCD messages, configures the limit switch pin, enables the stepper,
   * and resets internal flags and accumulators for the homing and baseline
   * measurement sequence.
   */
  void begin() override;

  /**
   * @brief Execute one non-blocking step of the HOME mode state machine.
   *
   * Drives the homing sequence and, once homed, manages the baseline current
   * measurement phase. This function should be called repeatedly until it
   * returns true, indicating that homing and baseline measurement are complete.
   *
   * @return true  if HOME mode has completed and control can return to the menu,
   * @return false otherwise.
   */
  bool step() override;

  /**
   * @brief Cleanup performed when exiting HOME mode.
   *
   * Typically ensures the stepper driver remains in a safe, enabled state.
   */
  void end() override;

private:
  /** @brief Reference to the LCD used for status messages. */
  Lcd1602& lcd_;

  /** @brief Reference to the stepper driver implementing motion control. */
  StepperDriver& stepper_;

  /** @brief Digital input pin connected to the homing limit switch. */
  uint8_t limitPin_;

  /** @brief Reference to the current sensor for baseline measurement. */
  CurrentSensor& current_;  

  /** @brief Flag indicating whether the homing sequence has finished. */
  bool  homed_  = false;

  /** @brief Target Z position (e.g. 30 mm) for baseline measurement. */
  float target_ = 0.0f;

  /**
   * @brief Flag indicating that baseline current measurement is in progress.
   */
  bool baselineMeasuring_ = false;

  /**
   * @brief Flag indicating that baseline current measurement has finished.
   */
  bool baselineDone_ = false;
  
  /** @brief Start time (in ms) of the baseline measurement window. */
  unsigned long baselineStart_ = 0;

  /** @brief Accumulated sum of RMS current samples during baseline measurement. */
  float baselineSum_ = 0.0f;

  /** @brief Number of RMS samples accumulated into baselineSum_. */
  uint32_t baselineCount_ = 0;
};

/**
 * @brief Mode for surface detection and etching using a 30 V supply (MOD1).
 *
 * High-level behavior:
 *  - Move downward while monitoring corrected RMS current until a threshold
 *    indicates surface contact.
 *  - Perform controlled plunges and 30 V validation pulses to confirm contact.
 *  - Enter an etching phase with 30 V on, moving slowly upward while observing
 *    the current.
 *  - Once the current falls below a configurable threshold, stop etching and
 *    lift the tool by a fixed distance.
 *
 * The mode uses a small state machine (State) to manage its internal phases.
 */
class Mod1Mode : public IMode {
public:
  /**
   * @brief Construct a new Mod1Mode instance.
   *
   * @param lcd               Reference to the LCD helper for user feedback.
   * @param stepper           Reference to the stepper driver for Z motion.
   * @param relayPin1         First relay control pin (part of 30 V switching).
   * @param relayPin2         Second relay control pin (part of 30 V switching).
   * @param current           Reference to the current sensor used for detection.
   * @param currentThreshold  Threshold for surface detection based on current.
   * @param etchingThreshold  Threshold for terminating the etching phase.
   * @param Iavg              Long-window moving average for current monitoring.
   * @param IavgS             Short-window moving average for smoothing/detection.
   */
  Mod1Mode(Lcd1602& lcd, StepperDriver& stepper, uint8_t relayPin1, uint8_t relayPin2, CurrentSensor& current, float currentThreshold, float etchingThreshold, IAvg_t& Iavg, IAvg_s& IavgS)
  : lcd_(lcd), stepper_(stepper), relayPin1_(relayPin1), relayPin2_(relayPin2), current_(current), threshold_(currentThreshold), etchingThreshold_(etchingThreshold), Iavg_(Iavg), IavgS_(IavgS){}

  /**
   * @brief Get the name of this mode.
   *
   * @return Constant C-string "MOD1".
   */
  const char* name() const override { return "MOD1"; }

  /**
   * @brief Initialize MOD1 mode.
   *
   * Sets up output pins, internal state machine variables, current-averaging
   * filters, and initial motion parameters for downward surface search.
   */
  void begin() override;

  /**
   * @brief Execute one step of the MOD1 state machine.
   *
   * Performs non-blocking progression through the MOD1 phases: downward
   * detection, validation, etching, and final lift. This should be called
   * repeatedly until it returns true to signal mode completion.
   *
   * @return true  when the mode has completed and control may return to the menu,
   * @return false while the mode is still active.
   */
  bool step() override;

  /**
   * @brief Cleanup performed when leaving MOD1 mode.
   *
   * Ensures that the stepper is stopped and enabled, current measurement is
   * disabled, and relay outputs are placed in a safe (OFF) state.
   */
  void end() override;

private:
  /**
   * @brief Internal state machine for MOD1 mode.
   *
   * The sequence includes:
   *  - MovingDownDetect : downward motion searching for surface via current.
   *  - Wait1            : pause after surface detection.
   *  - MoveDown1        : additional plunge after surface.
   *  - Wait2            : pause before validation.
   *  - Validate30V      : 30 V validation period to confirm true contact.
   *  - RelayHold        : 30 V ON pre-etch phase.
   *  - Etching          : 30 V ON, slow upward etching with current monitoring.
   *  - MoveUpBump       : optional upward bump behavior (if used).
   *  - FinalLift        : final lift after etching is complete.
   *  - Done             : terminal state indicating mode completion.
   */
  enum class State : uint8_t {
    MovingDownDetect,
    Wait1,
    MoveDown1,
    Wait2,
    Validate30V,
    RelayHold,
    Etching,
    MoveUpBump,
    FinalLift,
    Done
  };

  /** @brief Reference to LCD for on-screen messages and prompts. */
  Lcd1602& lcd_;

  /** @brief Reference to the stepper driver controlling motion. */
  StepperDriver& stepper_;

  /** @brief First relay control pin used for high-voltage switching. */
  uint8_t relayPin1_;

  /** @brief Second relay control pin used for high-voltage switching. */
  uint8_t relayPin2_;

  /** @brief Reference to the current sensor for threshold-based decisions. */
  CurrentSensor& current_;

  /** @brief Surface detection threshold based on corrected RMS current. */
  float         threshold_;

  /** @brief Threshold for terminating etching based on current drop. */
  float etchingThreshold_;

  /** @brief Timestamp (ms) when 30 V was turned on (start of etching/hold). */
  unsigned long etchStart_ = 0;

  /** @brief Long-window moving average for current during etching/hold. */
  IAvg_t& Iavg_;

  /** @brief Short-window moving average for current during detection/validation. */
  IAvg_s& IavgS_;
  
  /** @brief Current state in the MOD1 state machine. */
  State st_ = State::MovingDownDetect;

  /** @brief Generic wait-start timestamp used in various timed states. */
  unsigned long waitStart_ = 0;

  /** @brief Timestamp (ms) marking the beginning of the 30 V validation phase. */
  unsigned long validateStart_ = 0;

  /** @brief Count of relay pulse cycles executed (if pulses are used). */
  uint8_t       pulseCount_  = 0;

  /** @brief Timestamp (ms) of the start of the current pulse phase. */
  unsigned long pulseStart_  = 0;

  /** @brief Flag indicating whether the relay is currently ON (true) or OFF (false). */
  bool          relayOn_     = false;

  /** @brief Timestamp (ms) when motion was stopped (useful for diagnostics). */
  unsigned long stopTime_   = 0;

  /**
   * @brief Flag indicating whether a single upward bump has already been applied.
   *
   * Used to ensure that any corrective +1 mm upward motion happens only once
   * when current drops below a given threshold (if that behavior is enabled).
   */
  bool bumpedUp1mm_ = false;
};

/**
 * @brief Mode for surface detection, etching validation, and pulsed 9 V processing (MOD2).
 *
 * High-level behavior:
 *  - Detect surface via current threshold while moving downward.
 *  - Perform a controlled plunge and a short 30 V validation pulse.
 *  - Once validated, hold 30 V for a period with current monitoring and then
 *    switch it off when a condition is met.
 *  - Execute additional downward motion and finally apply a series of 9 V
 *    pulses configured by Parameters.
 *  - Lift the tool by a fixed amount and finish.
 */
class Mod2Mode : public IMode {
public:
  /**
   * @brief Construct a new Mod2Mode instance.
   *
   * @param lcd               Reference to the LCD for messages and prompts.
   * @param stepper           Reference to the stepper driver for Z motion.
   * @param relayPin1         First relay control pin (part of 30 V / 9 V switching).
   * @param relayPin2         Second relay control pin (part of 30 V / 9 V switching).
   * @param current           Reference to the current sensor used for detection.
   * @param surfaceThreshold  Threshold for initial surface detection.
   * @param etchingThreshold  Threshold for decisions during etching/hold phases.
   * @param Iavg              Long-window moving average for current.
   * @param IavgS             Short-window moving average for current.
   */
  Mod2Mode(Lcd1602& lcd,
           StepperDriver& stepper,
           uint8_t relayPin1,
           uint8_t relayPin2,
           CurrentSensor& current,
           float surfaceThreshold,
           float etchingThreshold,
           IAvg_t& Iavg,
           IAvg_s& IavgS)
    : lcd_(lcd),
      stepper_(stepper),
      relayPin1_(relayPin1),
      relayPin2_(relayPin2),
      current_(current),
      threshold_(surfaceThreshold),
      etchingThreshold_(etchingThreshold),
      Iavg_(Iavg),
      IavgS_(IavgS) {}

  /**
   * @brief Get the name of this mode.
   *
   * @return Constant C-string "MOD2".
   */
  const char* name() const override { return "MOD2"; }

  /**
   * @brief Initialize MOD2 mode.
   *
   * Configures relay pins, state machine variables, current-averaging filters,
   * and motion parameters for downward surface search and subsequent processing.
   */
  void begin() override;

  /**
   * @brief Execute one step of the MOD2 state machine.
   *
   * Drives MOD2 through its phases: surface detection, validation, 30 V hold,
   * additional motion, 9 V pulse sequence, and final lift. Non-blocking; call
   * repeatedly until it returns true.
   *
   * @return true  when MOD2 has completed and control should return to the menu,
   * @return false while the mode is still running.
   */
  bool step() override;

  /**
   * @brief Cleanup performed when leaving MOD2 mode.
   *
   * Stops and enables the stepper, disables current measurement, and switches
   * relays to a safe OFF configuration.
   */
  void end() override;

private:
  /**
   * @brief Internal state machine for MOD2 mode.
   *
   * States:
   *  - MovingDownDetect : downward motion looking for surface via current.
   *  - Wait1            : delay after initial detection.
   *  - MoveDown1        : additional downward move.
   *  - Wait2            : pause before validation.
   *  - Validate30V      : short 30 V validation pulse to confirm contact.
   *  - RelayHold        : hold 30 V, monitor current.
   *  - Wait3            : pause after turning 30 V off.
   *  - MoveDown2        : additional downward motion after etch.
   *  - Wait4            : pause before starting 9 V pulses.
   *  - RelayPulse       : series of 9 V ON/OFF pulses.
   *  - FinalLift        : final upward motion applied at the end.
   *  - Done             : terminal state indicating completion.
   */
  enum class State : uint8_t {
    MovingDownDetect,
    Wait1,
    MoveDown1,
    Wait2,
    Validate30V,
    RelayHold,
    Wait3,
    MoveDown2,
    Wait4,
    RelayPulse,
    FinalLift,
    Done
  };

  /** @brief Reference to the LCD for user-facing messages. */
  Lcd1602& lcd_;

  /** @brief Reference to the stepper driver for Z motion. */
  StepperDriver& stepper_;

  /** @brief First relay control pin (HV switching). */
  uint8_t relayPin1_;

  /** @brief Second relay control pin (HV switching). */
  uint8_t relayPin2_;

  /** @brief Reference to the current sensor used for current-based thresholds. */
  CurrentSensor& current_;

  /** @brief Threshold for initial surface detection based on current. */
  float threshold_;

  /** @brief Threshold for decisions during etching/hold stages. */
  float etchingThreshold_;

  /** @brief Long-window moving average for current. */
  IAvg_t& Iavg_;

  /** @brief Short-window moving average for current. */
  IAvg_s& IavgS_;

  /** @brief Current state in the MOD2 state machine. */
  State st_ = State::MovingDownDetect;

  /** @brief Generic timestamp (ms) used for various waits. */
  unsigned long waitStart_     = 0;

  /** @brief Timestamp (ms) marking the start of the 30 V validation phase. */
  unsigned long validateStart_ = 0;

  /** @brief Timestamp (ms) marking the start of the current pulse phase. */
  unsigned long pulseStart_    = 0;

  /** @brief Timestamp (ms) marking the start of the 30 V hold/etching phase. */
  unsigned long etchStart_     = 0;

  /** @brief Flag indicating whether the relay is currently ON (true) or OFF (false). */
  bool    relayOn_    = false;

  /** @brief Number of 9 V pulse cycles executed. */
  uint8_t pulseCount_ = 0;
};

/**
 * @brief Manual jogging mode for up/down movement with keypad (JOG).
 *
 * Behavior:
 *  - UP/DOWN keys move the Z axis in opposite directions at a fixed speed,
 *    constrained by configured soft limits.
 *  - The current position is periodically displayed on the LCD.
 *  - SELECT exits the mode and returns control to the higher-level controller.
 */
class JogMode : public IMode {
public:
  /**
   * @brief Construct a new JogMode instance.
   *
   * @param lcd      Reference to the LCD used for showing position and hints.
   * @param keys     Reference to the keypad for reading UP/DOWN/SELECT.
   * @param stepper  Reference to the stepper driver for motion control.
   */
  JogMode(Lcd1602& lcd, KeypadShield& keys, StepperDriver& stepper)
    : lcd_(lcd), keys_(keys), stepper_(stepper) {}

  /**
   * @brief Get the name of this mode.
   *
   * @return Constant C-string "JOG".
   */
  const char* name() const override { return "JOG"; }

  /**
   * @brief Initialize JogMode.
   *
   * Shows a hint on the LCD, enables the stepper driver, and resets internal
   * timing/flags, including a guard for ignoring the initial SELECT that may
   * have been used to enter the mode.
   */
  void begin() override;

  /**
   * @brief Execute one step of JOG behavior.
   *
   * Reads the keypad, applies motion within configured limits, updates the
   * LCD periodically with the current position, and exits when SELECT is held.
   *
   * @return true  if SELECT is pressed and the mode should exit,
   * @return false otherwise.
   */
  bool step() override;

  /**
   * @brief Cleanup performed when leaving JogMode.
   *
   * Stops the stepper and ensures it remains enabled in a safe state.
   */
  void end() override;

private:
  /** @brief Reference to the LCD for displaying jog information. */
  Lcd1602& lcd_;

  /** @brief Reference to the keypad used for jog commands. */
  KeypadShield& keys_;

  /** @brief Reference to the stepper driver that is being jogged. */
  StepperDriver& stepper_;

  /**
   * @brief Timestamp (ms) used to pace LCD updates.
   *
   * Helps avoid refreshing the display on every loop iteration, updating only
   * at a specified interval (e.g. every 200 ms).
   */
  uint32_t uiTick_ = 0;

  /**
   * @brief Flag indicating that the next step call is the first one.
   *
   * Used to ignore a SELECT press that may have been used to enter JogMode,
   * preventing immediate exit.
   */
  bool firstStep_ = true;
};
