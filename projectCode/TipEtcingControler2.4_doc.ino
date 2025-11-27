#include <Arduino.h>
#include "Lcd1602.h"
#include "KeypadShield.h"
#include "Modes.h"
#include "ModeController.h"
#include "StepperDriver.h"
#include "CurrentSensor.h"
#include "MovingAverage.h"
#include "ParametersMode.h"
#include "Parameters.h"

/**
 * @file main.ino
 * @brief Main application entry point for an Arduino-based etching/jogging system.
 *
 * This sketch wires together:
 *  - a 16x2 LCD (Lcd1602),
 *  - a keypad shield (KeypadShield),
 *  - a TMC2209-based stepper driver (StepperDriver),
 *  - a current sensor (CurrentSensor) with RMS/AC processing,
 *  - several logical operating modes (HomeMode, Mod1Mode, Mod2Mode, JogMode, ParametersMode),
 *  - and a ModeController implementing a simple menu-driven UI.
 *
 * Runtime behavior:
 *  - setup() initializes all peripherals, sensors, and modes, and starts the mode controller
 *    (which automatically enters HOME mode).
 *  - loop() periodically updates the current sensor and mode controller, ensuring non-blocking
 *    behavior for motion control and user interaction.
 */

// ---------------------- Pin mappings ----------------------

/** @name LCD and keypad shield pins (DFR0009 mapping)
 *  @{
 */
constexpr uint8_t LCD_D4 = 4;
constexpr uint8_t LCD_D5 = 5;
constexpr uint8_t LCD_D6 = 6;
constexpr uint8_t LCD_D7 = 7;
constexpr uint8_t LCD_RS = 8;
constexpr uint8_t LCD_EN = 9;
constexpr uint8_t LCD_BL = 10; ///< PWM-capable pin for LCD backlight.
constexpr uint8_t BTN_ADC = A0;///< Analog input used by keypad shield resistor ladder.
/** @} */

/** @name Stepper driver pins (TMC2209 STEP/DIR/EN)
 *  @{
 *
 * These pins are chosen to avoid conflicts with the LCD shield.
 */
constexpr uint8_t PIN_STEP = 12;
constexpr uint8_t PIN_DIR  = 13;
constexpr uint8_t PIN_EN   = 11; ///< Active-LOW enable signal.
/** @} */

/** @name Mechanical parameters
 *  @{
 *
 * These parameters describe the mechanics of the axis and must be adapted
 * to the actual hardware:
 *  - motor step count per revolution,
 *  - microstepping factor,
 *  - lead-screw pitch (mm per revolution),
 *  - maximum usable linear speed.
 */
constexpr float STEPS_PER_REV = 200.0f; ///< Typical for a 1.8° NEMA17 stepper.
constexpr int   MICROSTEPS    = 16;     ///< Microstepping factor for the TMC2209.
constexpr float LEAD_MM       = 8.0f;   ///< Lead screw pitch in mm/revolution.
constexpr float MAX_MM_S      = 10.0f;  ///< Maximum jog speed in mm/s.
/** @} */

/** @name Limit switch
 *  @{
 *
 * Digital input used as the homing/limit switch for the axis.
 */
constexpr uint8_t PIN_LIMIT = 2;
/** @} */

/** @name Relay pins
 *  @{
 *
 * Analog pins used as digital outputs driving external relays for 30 V / 9 V
 * switching in the etching modes.
 */
constexpr uint8_t PIN_RELAY1 = A1;
constexpr uint8_t PIN_RELAY2 = A2;
/** @} */

/** @name Current measurement and thresholds
 *  @{
 *
 * CurrentSensor measures the RMS current on PIN_I_SENSOR. The thresholds here
 * control surface detection and etching termination criteria.
 */
float baselineCurrent = 0.0f;          ///< Baseline RMS current measured in HOME mode.
const uint8_t PIN_I_SENSOR = A3;       ///< Analog input used for the current sensor.
const float I_THRESHOLD = 0.05f;       ///< Surface detection threshold (A).
const float I_ETCHING_THRESHOLD1 = 0.05f; ///< Etching threshold for MOD1 (A).
const float I_ETCHING_THRESHOLD2 = 0.05f; ///< Etching threshold for MOD2 (A).
/** @} */

// ---------------------- Peripheral instances ----------------------

/**
 * @brief Global LCD instance for user interface output.
 *
 * Uses a 4-bit parallel interface plus a PWM backlight pin.
 */
Lcd1602 lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7, LCD_BL, /*invertBL=*/false);

/**
 * @brief Global keypad shield instance for button input.
 *
 * The second argument specifies the debounce time in milliseconds.
 */
KeypadShield keys(BTN_ADC, 80);

/**
 * @brief Global stepper driver for the Z axis.
 *
 * Configured with the mechanical and electrical characteristics defined above.
 */
StepperDriver stepper(PIN_STEP, PIN_DIR, PIN_EN, STEPS_PER_REV, MICROSTEPS, LEAD_MM, MAX_MM_S);

/**
 * @brief Global current sensor instance used for surface detection and etching logic.
 *
 * Parameters:
 *  - analog pin,
 *  - reference voltage,
 *  - ADC maximum value,
 *  - calibration factor (A/V),
 *  - sampling window and sampling interval (microseconds).
 */
CurrentSensor currentSensor(PIN_I_SENSOR, 5.0f, 1023.0f, 2.545f, 40000UL, 200UL);

// ---------------------- RMS helpers for current ----------------------

/**
 * @brief Long-window moving average for current (e.g. slow etching monitoring).
 */
IAvg_t Iavg;

/**
 * @brief Short-window moving average for current (e.g. fast surface detection).
 */
IAvg_s IavgS;

// ---------------------- Mode instances ----------------------

/**
 * @brief HOME mode: homing sequence + baseline current measurement.
 */
HomeMode home(lcd, stepper, PIN_LIMIT, currentSensor);

/**
 * @brief MOD1: surface detection and etching with continuous 30 V.
 */
Mod1Mode mod1 (lcd,
               stepper,
               PIN_RELAY1,
               PIN_RELAY2,
               currentSensor,
               I_THRESHOLD,
               I_ETCHING_THRESHOLD1,
               Iavg,
               IavgS);

/**
 * @brief MOD2: surface detection and pulsed processing (30 V validation, 9 V pulses).
 */
Mod2Mode mod2 (lcd,
               stepper,
               PIN_RELAY1,
               PIN_RELAY2,
               currentSensor,
               I_THRESHOLD,
               I_ETCHING_THRESHOLD2,
               Iavg,
               IavgS);

/**
 * @brief Jog mode: manual UP/DOWN jog with position display.
 */
JogMode   jog(lcd, keys, stepper);

/**
 * @brief Parameter mode: interactive editor for MOD1/MOD2 parameters.
 */
ParametersMode paramMode(lcd, keys);

/**
 * @brief Array of all available modes in menu order.
 *
 * Modes:
 *  0. HOME        - automatic homing and baseline current acquisition.
 *  1. MOD1        - surface detection + continuous etching.
 *  2. MOD2        - surface detection + validation + pulsed processing.
 *  3. JOG         - manual jog mode.
 *  4. PARAM       - parameter editor.
 */
IMode* modes[] = { &home, &mod1, &mod2, &jog, &paramMode };

/**
 * @brief Global mode controller handling menu navigation and mode execution.
 *
 * It uses the LCD for display output, the keypad for navigation, and the modes
 * array above as the list of selectable/launchable modes.
 */
ModeController ctrl(lcd, keys, modes, 5);

// ---------------------- Arduino lifecycle ----------------------

/**
 * @brief Arduino setup function.
 *
 * Performs one-time initialization of:
 *  - serial port (for optional diagnostics),
 *  - LCD (backlight and geometry),
 *  - keypad debounce state,
 *  - current sensor (sampling state),
 *  - mode controller (which automatically starts HOME mode).
 */
void setup() {
  Serial.begin(115200);

  lcd.begin();
  keys.begin();
  currentSensor.begin();
  ctrl.begin();    // HOME starts automatically
}

/**
 * @brief Arduino main loop.
 *
 * This function:
 *  - updates the current sensor (non-blocking, time-window based),
 *  - advances the mode state machine through ModeController::loop().
 *
 * It must run as frequently as possible to ensure responsive UI and smooth
 * stepping. No blocking delays should be introduced here.
 */
void loop() {
  currentSensor.update();
  ctrl.loop();
}
