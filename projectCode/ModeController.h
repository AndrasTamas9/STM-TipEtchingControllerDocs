#pragma once
#include "KeypadShield.h"
#include "IMode.h"
#include "Lcd1602.h"

/**
 * @brief State machine for menu navigation and mode execution.
 *
 * ModeController coordinates:
 *  - rendering a mode-selection menu on an LCD display,
 *  - processing keypad input to select and start modes,
 *  - running the currently active mode in a non-blocking fashion.
 *
 * It manages an array of IMode objects, each representing a separate logical
 * operating mode of the system. The controller switches between a menu state
 * (where the user selects a mode) and a running state (where a single mode is
 * active and its step() function is called repeatedly).
 */
class ModeController {
public:
  /**
   * @brief User interface state of the controller.
   *
   * - MENU:    The LCD shows the list of available modes; the user can navigate
   *            and select one.
   * - RUNNING: One mode is currently active and receives step() calls.
   */
  enum class UiState : uint8_t { MENU, RUNNING };

  /**
   * @brief Construct a new ModeController instance.
   *
   * @param lcd        Reference to the LCD helper used to render the menu and status.
   * @param keys       Reference to the keypad handler used for user input.
   * @param modes      Pointer to an array of IMode* representing all available modes.
   * @param modeCount  Number of elements in the modes array.
   */
  ModeController(Lcd1602& lcd, KeypadShield& keys, IMode** modes, uint8_t modeCount);

  /**
   * @brief Initialize the controller and start operation.
   *
   * This function is typically called once from setup(). It initializes the
   * internal state, draws the initial menu, and may start a default mode
   * depending on the implementation in the source file.
   */
  void begin();

  /**
   * @brief Main update function to be called from the Arduino loop().
   *
   * Responsibilities:
   *  - Reads keypad events.
   *  - In MENU state: updates the selected mode and handles mode activation.
   *  - In RUNNING state: calls step() on the active mode and interprets its
   *    completion condition or exit key.
   *
   * The method is non-blocking and should be called frequently.
   */
  void loop();

private:
  /**
   * @brief Draw the current menu screen on the LCD.
   *
   * This function shows a title and the currently selected mode name. It is
   * used whenever the menu needs to be refreshed after navigation or after a
   * mode exits.
   */
  void drawMenu_();

  /**
   * @brief Start the mode at the given index.
   *
   * Sets the internal running mode index, invokes begin() on the selected mode,
   * and switches the UI state to RUNNING.
   *
   * @param idx Index into the modes_ array (0 <= idx < n_).
   */
  void start_(uint8_t idx);

  /**
   * @brief Stop the currently running mode and return to the menu state.
   *
   * Invokes end() on the active mode, updates the UI state to MENU, and
   * refreshes the menu on the LCD.
   */
  void stop_();

  /** @brief Reference to the LCD handler used for all on-screen output. */
  Lcd1602& lcd_;

  /** @brief Reference to the keypad handler used for user navigation and selection. */
  KeypadShield& keys_;

  /** @brief Array of pointers to all available modes. */
  IMode** modes_;

  /** @brief Number of modes stored in modes_. */
  uint8_t n_;

  /** @brief Current user-interface state (MENU or RUNNING). */
  UiState ui_ = UiState::MENU;

  /** @brief Index of the currently selected mode in the menu. */
  uint8_t selected_ = 0;

  /** @brief Index of the mode that is currently running. */
  uint8_t running_ = 0;
};
