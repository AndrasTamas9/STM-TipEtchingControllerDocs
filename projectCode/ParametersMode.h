#pragma once
#include "IMode.h"
#include "Lcd1602.h"
#include "KeypadShield.h"
#include "Parameters.h"
#include <Arduino.h>

/**
 * @brief Interactive on-device editor for MOD1/MOD2 parameters.
 *
 * ParametersMode implements an IMode-based, non-blocking UI to inspect and edit
 * the configurable parameters stored in gParams (for MOD1 and MOD2). It uses:
 *
 * - A 16x2 LCD (Lcd1602) to display the current screen:
 *   - mode selection: choose MOD1 or MOD2,
 *   - parameter selection: choose which parameter to edit,
 *   - float or integer editors: digit-by-digit editing.
 * - A KeypadShield for input (UP, DOWN, LEFT, RIGHT, SELECT).
 *
 * Navigation / interaction summary:
 * - In mode selection:
 *     - UP/DOWN/LEFT/RIGHT: toggle between MOD1 and MOD2.
 *     - Short SELECT: enter parameter selection for the chosen mode.
 *     - Long SELECT (>= 2 s): exit ParametersMode (step() returns true).
 * - In parameter selection:
 *     - UP/DOWN: move between parameters of the current mode.
 *     - Short SELECT: enter editor (float or int depending on parameter).
 *     - Long SELECT: return to mode selection.
 * - In editors:
 *     - LEFT/RIGHT: move cursor between digits.
 *     - UP/DOWN: increment/decrement the digit under the cursor.
 *     - Short SELECT: save and return to parameter selection.
 *     - Long SELECT: save and return to mode selection.
 */
class ParametersMode : public IMode {
public:
    /**
     * @brief Construct a new ParametersMode instance.
     *
     * @param lcd   Reference to the LCD used for all parameter UI screens.
     * @param keys  Reference to the keypad used for editing and navigation.
     */
    ParametersMode(Lcd1602& lcd, KeypadShield& keys)
        : lcd_(lcd), keys_(keys) {}

    /**
     * @brief Name of this mode.
     *
     * @return Constant C-string "PARAM".
     */
    const char* name() const override { return "PARAM"; }

    /**
     * @brief Initialize the parameter editing mode.
     *
     * Resets the internal state machine to mode-selection, clears the display,
     * initializes selection indices and flags, and draws the initial
     * "Select MODE" screen.
     */
    void begin() override;

    /**
     * @brief Execute one non-blocking step of the parameter editing logic.
     *
     * Reads the current stable key state from the keypad, updates the internal
     * UI state (mode selection, parameter selection, or editor), performs
     * long-press detection for SELECT, and updates the LCD as needed.
     *
     * @return true  if the user has requested to exit parameter mode
     *               (long SELECT in mode-selection state),
     * @return false while parameter mode should continue running.
     */
    bool step() override;

    /**
     * @brief Cleanup when leaving parameter mode.
     *
     * Currently clears the LCD; this is a hook where persistent storage or
     * additional actions can be triggered if desired.
     */
    void end() override;

private:
    /**
     * @brief Internal state machine states for the parameter editor UI.
     *
     * - SelectMode : top level, choose whether to edit MOD1 or MOD2.
     * - SelectParam: list of parameters for the selected mode.
     * - EditFloat  : digit-based float editor ("XXX.XXX").
     * - EditInt    : digit-based integer editor ("DDD").
     */
    enum class State {
        SelectMode,
        SelectParam,
        EditFloat,
        EditInt
    };

    /** @brief LCD used for all user interface output. */
    Lcd1602&      lcd_;

    /** @brief Keypad used for all navigation and editing input. */
    KeypadShield& keys_;

    /** @brief Current UI state (mode selection, parameter selection, or editor). */
    State state_ = State::SelectMode;

    /** @brief Selected mode index: 0 = MOD1, 1 = MOD2. */
    int selectedMode_  = 0;

    /** @brief Selected parameter index within the current mode. */
    int selectedParam_ = 0;

    /**
     * @brief Buffer for the float editor representing "XXX.XXX".
     *
     * Layout:
     *  - digits_[0..2]: integer digits (hundreds, tens, ones).
     *  - digits_[3]   : '.' decimal separator.
     *  - digits_[4..6]: fractional digits (tenths, hundredths, thousandths).
     */
    char digits_[7];

    /**
     * @brief Cursor position in the float editor (0..6).
     *
     * Values:
     *  - 0..2: integer digits,
     *  - 3   : decimal point (not editable),
     *  - 4..6: fractional digits.
     */
    int  cursor_ = 0;

    /**
     * @brief Buffer for the integer editor representing "DDD".
     *
     * Each entry holds a single digit character: hundreds, tens, ones.
     */
    char idigits_[3];

    /**
     * @brief Cursor position in the integer editor (0..2).
     */
    int  icursor_ = 0;

    /**
     * @brief Timestamp (ms) when SELECT was pressed down for long-press detection.
     */
    unsigned long selDownMs_ = 0;

    /**
     * @brief Flag indicating whether SELECT is currently being held down.
     */
    bool          selHeld_   = false;

    /**
     * @brief Flag that indicates the current screen needs to be redrawn.
     *
     * When set to true, the next step() call will refresh the LCD content for
     * the current state, and then reset this flag to false.
     */
    bool needRedraw_ = true;

    /**
     * @brief Last stable key state for edge/transition detection.
     */
    Key  lastKey_    = Key::NONE;

    /**
     * @brief Flag indicating the first call to step() after begin().
     *
     * Used to discard the SELECT press that might have been used to enter
     * this mode, avoiding immediate exit or unintended actions.
     */
    bool firstStep_  = true;

    /**
     * @brief Timestamp (ms) used for cursor blink timing.
     */
    unsigned long blinkTs_    = 0;

    /**
     * @brief Cursor blink state: true = block character, false = actual digit.
     */
    bool          blinkBlock_ = false;

    /**
     * @brief Flag to ignore the next SELECT release after a long-press event.
     *
     * When a long press triggers a state change (e.g., exit to mode selection),
     * the corresponding SELECT release event should not be interpreted as a
     * short press. This flag prevents such accidental transitions.
     */
    bool ignoreSelectRelease_;

    // --- internal helpers ---

    /**
     * @brief Draw the MODE selection screen (MOD1 vs MOD2) on the LCD.
     *
     * Uses selectedMode_ to determine which mode is highlighted.
     */
    void drawSelectMode();

    /**
     * @brief Draw the currently selected parameter and its value.
     *
     * Uses selectedMode_ and selectedParam_ to look up the appropriate
     * parameter name and value in gParams and display them on the LCD.
     */
    void drawSelectParam();

    /**
     * @brief Initialize the float editor with an initial value.
     *
     * Clamps the value to [0.000, 999.999], converts it to a fixed "XXX.XXX"
     * representation in digits_, resets the cursor and flags the editor for
     * redraw.
     *
     * @param value Initial floating-point value to edit.
     */
    void startEditFloat(float value);

    /**
     * @brief Convert the current float editor digits into a floating-point value.
     *
     * @return The value represented by digits_ in units of "XXX.XXX".
     */
    float floatFromDigits() const;

    /**
     * @brief Initialize the integer editor with an initial value.
     *
     * Clamps the value to [0, 999] and writes it into idigits_ as "DDD". Also
     * resets the cursor and requests a redraw.
     *
     * @param value Initial integer value to edit.
     */
    void startEditInt(int value);

    /**
     * @brief Convert the current integer editor digits into an integer value.
     *
     * @return The integer represented by idigits_ (0..999).
     */
    int  intFromDigits() const;

    /**
     * @brief Update the float editor based on the given key and redraw.
     *
     * Responds to UP/DOWN/LEFT/RIGHT by updating digits_ and cursor, then
     * redraws the parameter label and the "XXX.XXX" value on the LCD.
     *
     * @param k Current key (UP/DOWN/LEFT/RIGHT or NONE).
     */
    void updateFloatEditor(Key k);

    /**
     * @brief Update the integer editor based on the given key and redraw.
     *
     * Responds to UP/DOWN/LEFT/RIGHT by updating idigits_ and cursor, then
     * redraws the parameter label and "DDD" value on the LCD.
     *
     * @param k Current key (UP/DOWN/LEFT/RIGHT or NONE).
     */
    void updateIntEditor(Key k);

    /**
     * @brief Detect a long SELECT press based on current key and timestamp.
     *
     * When the stable key is SELECT, this function:
     *  - Starts timing when first pressed (selHeld_ = true, selDownMs_ set),
     *  - Returns true once the duration exceeds 2000 ms,
     *  - Resets selHeld_ after reporting a long-press event.
     * Any non-SELECT key resets selHeld_ and cancels a pending long press.
     *
     * @param stableKey  Current stable key state from the keypad.
     * @param now        Current timestamp in milliseconds from millis().
     * @return true  if a long-press event has been detected,
     * @return false otherwise.
     */
    bool checkLongPress(Key stableKey, unsigned long now);
};
