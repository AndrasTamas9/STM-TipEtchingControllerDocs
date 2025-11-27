#pragma once
#include <Arduino.h>

#define SIMULATE_KEYS 0

/**
 * @brief Logical keys available on the DFR0009 keypad (or similar shields).
 *
 * The keypad is connected via an analog voltage divider to a single analog pin.
 * Each button press maps to one of these logical key values based on the ADC
 * reading and configured thresholds.
 */
enum class Key : uint8_t { NONE, RIGHT, UP, DOWN, LEFT, SELECT };

/**
 * @brief Non-blocking keypad handler for analog-resistor ladder shields.
 *
 * KeypadShield reads an analog input connected to a keypad (e.g. DFR0009) where
 * each key press produces a distinct voltage level. Internally, it:
 *  - classifies raw ADC readings into Key values using configurable thresholds,
 *  - applies time-based debouncing,
 *  - provides edge-triggered key events via poll(),
 *  - exposes the current stable key state via stable().
 *
 * The class is intended to be polled frequently from the main loop without
 * blocking delays.
 */
class KeypadShield {
  
public:
  /**
   * @brief Construct a new KeypadShield object.
   *
   * @param analogPin   Analog pin connected to the keypad resistor ladder.
   * @param debounceMs  Debounce interval in milliseconds used to confirm that a
   *                    key state is stable before generating events.
   */
  explicit KeypadShield(uint8_t analogPin, uint16_t debounceMs = 50);

  /**
   * @brief Initialize the keypad shield.
   *
   * This method should be called once (typically in setup()). It resets the
   * internal key state and initializes the debounce timer.
   */
  void begin();

  /**
   * @brief Poll the keypad and return a single key-press edge event.
   *
   * This function:
   *  - reads the analog input,
   *  - classifies the reading as a logical key,
   *  - applies debouncing based on debounceMs,
   *  - returns a key value only when a new stable key press is detected.
   *
   * It returns Key::NONE if no new key press event has occurred since the
   * previous call. It does not block and should be called regularly in the main
   * loop.
   *
   * @return The newly detected key press (e.g. Key::RIGHT, Key::SELECT) or
   *         Key::NONE if no new event is available.
   */
  Key poll();

  /**
   * @brief Get the current stable key state.
   *
   * This returns the debounced key that is currently considered pressed. It
   * remains non-NONE as long as the key is held down and the analog reading
   * remains within the corresponding threshold range.
   *
   * @return The current stable key (or Key::NONE if no key is pressed).
   */
  Key stable() const { return stable_; }

  /**
   * @brief Set the ADC thresholds used to classify keys.
   *
   * The keypad usually forms a resistor ladder: each button press results in a
   * distinct analog value. These thresholds partition the ADC range into
   * segments associated with each key. They are expected to be in ascending
   * order.
   *
   * @param right   Threshold value for the RIGHT key.
   * @param up      Threshold value for the UP key.
   * @param down    Threshold value for the DOWN key.
   * @param left    Threshold value for the LEFT key.
   * @param select  Threshold value for the SELECT key.
   */
  void setThresholds(int right, int up, int down, int left, int select);

  /**
   * @brief Clear all stored key state and reset debouncing.
   *
   * After calling this, the keypad behaves as if no key has previously been
   * pressed: stable_ and last_ are reset to Key::NONE, and the debounce timer
   * is restarted.
   */
  void clear();

private:
  /**
   * @brief Classify a raw ADC reading into a Key value.
   *
   * The given ADC reading is compared against the stored thresholds in order.
   * The first threshold that is greater than the reading determines the key.
   * If the reading is greater than all thresholds, Key::NONE is returned.
   *
   * @param adc  Raw ADC reading.
   * @return Corresponding key or Key::NONE if no valid key is matched.
   */
  Key classify_(int adc) const;

  /** @brief Analog pin connected to the keypad resistor ladder. */
  uint8_t aPin_;

  /** @brief Debounce interval in milliseconds. */
  uint16_t dbMs_;

  /** @brief Last time (in ms) when the raw key state changed, used for debouncing. */
  uint32_t lastChange_ = 0;

  /** @brief Current stable (debounced) key state. */
  Key stable_ = Key::NONE;

  /** @brief Last instantaneous (raw, non-debounced) key state. */
  Key last_ = Key::NONE;

  /** @brief ADC thresholds used to distinguish keys. */
  int thRight_ = 60, thUp_ = 200, thDown_ = 400, thLeft_ = 600, thSel_ = 800;
};
