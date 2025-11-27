#include "KeypadShield.h"

/**
 * @file KeypadShield.cpp
 * @brief Implementation of the KeypadShield class for reading button presses via an analog input.
 *
 * The KeypadShield class reads a resistor-ladder style keypad connected to a single
 * analog input pin. Each button press produces a distinct voltage level, which is
 * mapped to a key code using configurable ADC thresholds. The class includes
 * debouncing logic and provides edge-triggered events for newly pressed keys.
 */

/**
 * @brief Construct a new KeypadShield object.
 *
 * @param analogPin  Analog pin connected to the keypad resistor ladder.
 * @param debounceMs Debounce time in milliseconds used to confirm a stable key state.
 */
KeypadShield::KeypadShield(uint8_t analogPin, uint16_t debounceMs)
  : aPin_(analogPin), dbMs_(debounceMs) {}

/**
 * @brief Initialize the keypad shield state.
 *
 * This method should be called once (typically in setup()). It resets the internal
 * stable and last key states and initializes the timestamp used for debouncing.
 */
void KeypadShield::begin(){
  stable_ = last_ = Key::NONE; 
  lastChange_ = millis();
}

/**
 * @brief Configure ADC thresholds for each key.
 *
 * The keypad is typically implemented as a resistor ladder where each button press
 * produces a different analog value. This function sets the upper threshold for
 * each key region in ascending order. The classification logic compares the ADC
 * reading against these thresholds to determine which key is pressed.
 *
 * @param right   Threshold for the RIGHT key.
 * @param up      Threshold for the UP key.
 * @param down    Threshold for the DOWN key.
 * @param left    Threshold for the LEFT key.
 * @param select  Threshold for the SELECT key.
 */
void KeypadShield::setThresholds(int right,int up,int down,int left,int select){
  thRight_=right; 
  thUp_=up; 
  thDown_=down;
  thLeft_=left; 
  thSel_=select;
}

/**
 * @brief Classify a raw ADC reading into a Key enum value.
 *
 * The thresholds define contiguous intervals associated with each button. The
 * reading is compared in ascending order; the first threshold exceeded determines
 * the key. If the value is above all thresholds, Key::NONE is returned.
 *
 * @param a  Raw ADC reading (0..ADC max).
 * @return Corresponding Key value, or Key::NONE if no key matches.
 */
Key KeypadShield::classify_(int a) const {
  if (a < thRight_) return Key::RIGHT;
  if (a < thUp_) return Key::UP;
  if (a < thDown_) return Key::DOWN;
  if (a < thLeft_) return Key::LEFT;
  if (a < thSel_) return Key::SELECT;
  return Key::NONE;
}

/**
 * @brief Clear any stored key state and reset the debounce timer.
 *
 * After calling this function, the keypad behaves as if no key has been pressed
 * yet: the stable and last keys are set to Key::NONE and the debounce timer is
 * restarted.
 */
void KeypadShield::clear() {
  stable_ = last_ = Key::NONE;
  lastChange_ = millis();
}

/**
 * @brief Poll the keypad and return a newly detected key press (edge).
 *
 * This method performs a non-blocking read of the analog pin, classifies the
 * current raw key, and applies a debounce mechanism:
 *  - If the raw key state changes, the debounce timer is reset.
 *  - Only when the raw state remains unchanged for at least dbMs_ milliseconds
 *    is the stable state updated.
 *  - The method returns a non-NONE key only when a new stable key is detected
 *    (i.e., edge detection on key press). If no new key press event occurs,
 *    Key::NONE is returned.
 *
 * Typical usage: call poll() in the main loop and react only when the return
 * value is not Key::NONE.
 *
 * @return The newly detected key press (Key::RIGHT, Key::LEFT, etc.) or
 *         Key::NONE if no new key press occurred.
 */
Key KeypadShield::poll(){

/*
#if SIMULATE_KEYS
  if (Serial.available()) {
    char c = Serial.read();
    auto mapChar = [](char ch)->Key {
      switch (ch) {
        case 'r': return Key::RIGHT;
        case 'l': return Key::LEFT;
        case 'u': return Key::UP;
        case 'd': return Key::DOWN;
        case 's': return Key::SELECT;
        default:  return Key::NONE;
      }
    };

    if (c == '0') {
      // Release event – no key is pressed
      stable_ = last_ = Key::NONE;
      lastChange_ = millis();
      return Key::NONE;
    }

    Key k = mapChar(c);
    if (k != Key::NONE) {
      // Update stable state so that stable() works correctly for modes using it
      last_ = stable_ = k;
      lastChange_ = millis();
      return k;  // Generate an immediate edge event
    }
    // Unknown character: no event
    return Key::NONE;
  }

  // If there is no serial input, do not read the analog pin in simulation mode
  // (to avoid noise-based false keys).
  return Key::NONE;
#endif
*/

  Key fell = Key::NONE;
  Key raw = classify_(analogRead(aPin_));
  if (raw != last_) {
    last_ = raw; 
    lastChange_ = millis();
  }
  if ((millis() - lastChange_) >= dbMs_ && stable_ != last_) {
    Key prev = stable_; 
    stable_ = last_;
    if (prev != stable_ && stable_ != Key::NONE) fell = stable_;
  }
  return fell;
}
