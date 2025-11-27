#include "ParametersMode.h"

/**
 * @file ParametersMode.cpp
 * @brief Implementation of the ParametersMode class for on-device parameter editing.
 *
 * ParametersMode provides a hierarchical UI on a 16x2 LCD with keypad input to:
 *  - select which mode's parameters to edit (MOD1 / MOD2),
 *  - select a specific parameter within that mode,
 *  - edit floating-point and integer parameters using a digit-based editor,
 *  - support short-press vs long-press semantics for SELECT (save / navigate / exit).
 *
 * Navigation summary:
 *  - UP/DOWN/LEFT/RIGHT: move between modes, parameters, and digits.
 *  - Short SELECT: enter parameter list, enter editor, or save and go back.
 *  - Long SELECT: go up one level (e.g. from parameter list to mode selection)
 *                 or exit parameter mode entirely (from mode selection).
 */

// --- begin / end -----------------------------------------------------

/**
 * @brief Initialize the parameter editing mode.
 *
 * This function:
 *  - resets the internal state machine (mode selection, parameter selection),
 *  - clears UI flags and key state,
 *  - performs an initial LCD clear and draws the mode-selection screen.
 *
 * The initial selection is MODE = MOD1 and parameter index 0.
 */
void ParametersMode::begin() {
    state_         = State::SelectMode;
    selectedMode_  = 0;      // 0 = MOD1, 1 = MOD2
    selectedParam_ = 0;
    selHeld_       = false;
    needRedraw_    = true;
    lastKey_       = Key::NONE;
    firstStep_  = true;
    ignoreSelectRelease_ = false;

    lcd_.clear();
    drawSelectMode();
}

/**
 * @brief Cleanup when leaving parameter mode.
 *
 * Currently clears the LCD; additional persistence or actions could be added
 * here if parameters are to be saved to non-volatile memory.
 */
void ParametersMode::end() {
    lcd_.clear();
}

// -------------------------- helper functions ---------------------------

/**
 * @brief Draw the mode-selection screen (MOD1 / MOD2) on the LCD.
 *
 * Line 0: "Select MODE:"
 * Line 1: highlights the currently selected mode with a leading '>' symbol.
 *         - If selectedMode_ == 0: ">MOD1     MOD2"
 *         - If selectedMode_ == 1: " MOD1    >MOD2"
 */
void ParametersMode::drawSelectMode() {
    lcd_.clear();
    lcd_.setCursor(0, 0);
    lcd_.print("Select MODE:");

    lcd_.setCursor(0, 1);
    if (selectedMode_ == 0) {
        lcd_.print(">MOD1     MOD2");
    } else {
        lcd_.print(" MOD1    >MOD2");
    }
}

/**
 * @brief Return a human-readable parameter label for a given mode and index.
 *
 * @param mode  0 for MOD1, 1 for MOD2.
 * @param idx   Parameter index within the mode.
 * @return Null-terminated C-string containing the label. Returns an empty
 *         string if the index is out of range.
 */
static const char* paramName(int mode, int idx) {
    if (mode == 0) { // MOD1
        switch (idx) {
            case 0: return "M1 PLUNGE [mm]";
            case 1: return "M1 Ithr [A]";
            case 2: return "M1 RET SPD[mm/s]";
        }
    } else { // MOD2
        switch (idx) {
            case 0: return "M2 PLUNGE [mm]";
            case 1: return "M2 Ithr [A]";
            case 2: return "M2 PLUNGE2 [mm]";
            case 3: return "M2 PULSE NUM";
            case 4: return "M2 PULSE T [s]";
        }
    }
    return "";
}

/**
 * @brief Return the number of editable parameters for a given mode.
 *
 * @param mode  0 for MOD1, 1 for MOD2.
 * @return Number of parameters in that mode.
 */
static int paramCountForMode(int mode) {
    return (mode == 0 ? 3 : 5);
}

/**
 * @brief Draw the parameter-selection screen for the currently selected parameter.
 *
 * Line 0: parameter name from paramName().
 * Line 1: current value read from gParams (formatted as float or int).
 *         For MOD2 parameter index 4, prints "on/off" as "<on>/<off>".
 */
void ParametersMode::drawSelectParam() {
    lcd_.clear();
    lcd_.setCursor(0, 0);
    lcd_.print(paramName(selectedMode_, selectedParam_));

    lcd_.setCursor(0, 1);

    // print current value
    if (selectedMode_ == 0) {
        if      (selectedParam_ == 0) lcd_.print(gParams.mod1.plungeAfterSurface_mm, 3);
        else if (selectedParam_ == 1) lcd_.print(gParams.mod1.etchingThreshold_A, 3);
        else if (selectedParam_ == 2) lcd_.print(gParams.mod1.retractSpeed_mm_s, 3);
    } else {
        if      (selectedParam_ == 0) lcd_.print(gParams.mod2.plungeAfterSurface_mm, 3);
        else if (selectedParam_ == 1) lcd_.print(gParams.mod2.etchingThreshold_A, 3);
        else if (selectedParam_ == 2) lcd_.print(gParams.mod2.plungeAfterEtch_mm, 3);
        else if (selectedParam_ == 3) lcd_.print(gParams.mod2.pulseCount);
        else if (selectedParam_ == 4) {
            lcd_.print(gParams.mod2.pulseOn_s, 3);
            lcd_.print("/");
            lcd_.print(gParams.mod2.pulseOff_s, 3);
        }
    }
}

// ---- float editor ----

/**
 * @brief Initialize the float editor with a given starting value.
 *
 * The value is:
 *  - clamped to [0.000, 999.999],
 *  - converted to an integer representing thousandths (value * 1000),
 *  - split into integer and fractional parts and encoded into a character
 *    array (digits_) of length 7: "XXX.XXX".
 *
 * After this call, the cursor is placed at the first digit and the editor
 * needs to be redrawn.
 *
 * @param value Initial floating-point value for the editor.
 */
void ParametersMode::startEditFloat(float value) {
    // clamp to [0, 999.999]
    if (value < 0.0f)       value = 0.0f;
    if (value > 999.999f)   value = 999.999f;

    // use long to avoid 16-bit int overflow on AVR
    unsigned long v  = (unsigned long)(value * 1000.0f + 0.5f);
    unsigned int  ip = v / 1000;   // integer part 0..999
    unsigned int  fp = v % 1000;   // fractional part (thousandths) 0..999

    if (ip > 999) ip = 999;

    digits_[0] = '0' + (ip / 100) % 10;
    digits_[1] = '0' + (ip / 10)  % 10;
    digits_[2] = '0' + (ip / 1)   % 10;
    digits_[3] = '.';
    digits_[4] = '0' + (fp / 100) % 10;
    digits_[5] = '0' + (fp / 10)  % 10;
    digits_[6] = '0' + (fp / 1)   % 10;

    cursor_     = 0;
    needRedraw_ = true;
}

/**
 * @brief Convert the current editor digits back into a float value.
 *
 * This reconstructs the value from digits_ in the form:
 *   XXX.XXX = integer part + fractional part / 1000.
 *
 * @return Floating-point value represented by the current digits_.
 */
float ParametersMode::floatFromDigits() const {
    int ip = (digits_[0] - '0') * 100 +
             (digits_[1] - '0') * 10 +
             (digits_[2] - '0');

    int fp = (digits_[4] - '0') * 100 +
             (digits_[5] - '0') * 10 +
             (digits_[6] - '0');

    return ip + fp / 1000.0f;
}

/**
 * @brief Update the float editor state based on a key input and redraw the value.
 *
 * Behavior:
 *  - LEFT/RIGHT: move the cursor between positions 0..6 (including the dot).
 *  - UP/DOWN: increment/decrement the digit at the cursor position modulo 10
 *             (if the cursor is not on the decimal point).
 *  - Redraws:
 *      * Line 0: parameter name.
 *      * Line 1: the 7 characters from digits_ plus spaces to fill the line.
 *  - Places the LCD cursor at the current digit position.
 *
 * @param k  Key input (UP/DOWN/LEFT/RIGHT or NONE).
 */
void ParametersMode::updateFloatEditor(Key k) {
    // navigation and digit changes
    if (k == Key::LEFT && cursor_ > 0)   cursor_--;
    if (k == Key::RIGHT && cursor_ < 6)  cursor_++;

    if (k == Key::UP || k == Key::DOWN) {
        if (digits_[cursor_] != '.') {
            int d = digits_[cursor_] - '0';
            if (k == Key::UP)   d = (d + 1) % 10;
            if (k == Key::DOWN) d = (d + 9) % 10;
            digits_[cursor_] = '0' + d;
        }
    }

    // redraw line 0 (parameter name)
    lcd_.setCursor(0, 0);
    lcd_.print("                ");
    lcd_.setCursor(0, 0);
    lcd_.print(paramName(selectedMode_, selectedParam_));

    // redraw line 1 (current digits)
    lcd_.setCursor(0, 1);
    for (int i = 0; i < 7; ++i) {
        lcd_.write(digits_[i]);
    }
    for (int i = 7; i < 16; ++i) {
        lcd_.write(' ');
    }

    // move LCD cursor to current digit
    lcd_.setCursor(cursor_, 1);
}

// ---- int editor ----

/**
 * @brief Initialize the integer editor with a starting value.
 *
 * The value is clamped to [0, 999] and encoded into a three-digit array
 * idigits_ as "XYZ" (hundreds, tens, ones). The cursor is placed at the first
 * digit and a redraw is requested.
 *
 * @param value Initial integer value for the editor.
 */
void ParametersMode::startEditInt(int value) {
    if (value < 0)   value = 0;
    if (value > 999) value = 999;

    idigits_[0] = '0' + (value / 100) % 10;
    idigits_[1] = '0' + (value / 10)  % 10;
    idigits_[2] = '0' + (value / 1)   % 10;

    icursor_   = 0;
    needRedraw_ = true;
}

/**
 * @brief Convert the current integer editor digits back into an int.
 *
 * @return Integer reconstructed from idigits_.
 */
int ParametersMode::intFromDigits() const {
    return (idigits_[0] - '0') * 100 +
           (idigits_[1] - '0') * 10 +
           (idigits_[2] - '0');
}

/**
 * @brief Update the integer editor and redraw its content based on a key input.
 *
 * Behavior:
 *  - LEFT/RIGHT: move the cursor between the three digits.
 *  - UP/DOWN: increment/decrement the digit at the cursor modulo 10.
 *  - Redraw:
 *      * Clears the display,
 *      * Prints the parameter name on line 0,
 *      * Prints the 3-digit value on line 1.
 *
 * @param k Key input (UP/DOWN/LEFT/RIGHT or NONE).
 */
void ParametersMode::updateIntEditor(Key k) {
    if (k == Key::LEFT && icursor_ > 0)   icursor_--;
    if (k == Key::RIGHT && icursor_ < 2)  icursor_++;

    if (k == Key::UP || k == Key::DOWN) {
        int d = idigits_[icursor_] - '0';
        if (k == Key::UP)   d = (d + 1) % 10;
        if (k == Key::DOWN) d = (d + 9) % 10;
        idigits_[icursor_] = '0' + d;
    }

    lcd_.clear();
    lcd_.setCursor(0, 0);
    lcd_.print(paramName(selectedMode_, selectedParam_));
    lcd_.setCursor(0, 1);
    for (int i = 0; i < 3; ++i) lcd_.write(idigits_[i]);
}

// ---- long SELECT handling ----

/**
 * @brief Detect a long SELECT press based on the current stable key and time.
 *
 * Logic:
 *  - When SELECT is first detected, selHeld_ is set and selDownMs_ stores the
 *    timestamp.
 *  - As long as SELECT remains held, the function checks whether the hold
 *    duration exceeds 2000 ms.
 *  - On exceeding 2000 ms, selHeld_ is reset and the function returns true to
 *    indicate a long-press event.
 *  - Any non-SELECT key state resets selHeld_ and cancels the long-press.
 *
 * @param s    Current stable key.
 * @param now  Current time in milliseconds from millis().
 * @return true if a long-press event is detected, false otherwise.
 */
bool ParametersMode::checkLongPress(Key s, unsigned long now) {
    if (s == Key::SELECT) {
        if (!selHeld_) {
            selHeld_   = true;
            selDownMs_ = now;
        } else {
            if (now - selDownMs_ >= 2000UL) {
                selHeld_ = false;
                return true;
            }
        }
    } else {
        selHeld_ = false;
    }
    return false;
}

// -------------------------- main step() ---------------------------

/**
 * @brief Execute one step of the ParametersMode state machine.
 *
 * High-level state transitions:
 *  - State::SelectMode:
 *      * LEFT/RIGHT/UP/DOWN: toggles between MOD1 and MOD2.
 *      * Short SELECT release: enter State::SelectParam.
 *      * Long SELECT: exit ParametersMode (returns true).
 *
 *  - State::SelectParam:
 *      * UP/DOWN: move between parameters for the selected mode.
 *      * Short SELECT: enter the appropriate editor:
 *          - State::EditFloat for float parameters,
 *          - State::EditInt for integer parameters.
 *      * Long SELECT: go back to State::SelectMode.
 *
 *  - State::EditFloat:
 *      * UP/DOWN/LEFT/RIGHT: edit digits and move cursor.
 *      * Short SELECT: save the new value to gParams and return to State::SelectParam.
 *      * Long SELECT: save and return to State::SelectMode.
 *      * The editor also implements a cursor "blink" effect.
 *
 *  - State::EditInt:
 *      * UP/DOWN/LEFT/RIGHT: edit integer digits.
 *      * Short SELECT: save the integer and return to State::SelectParam.
 *      * Long SELECT: save and return to State::SelectMode.
 *      * A cursor blink effect is applied similarly to the float editor.
 *
 * The function is non-blocking and should be called periodically from the main
 * loop. It returns true only when the user performs a long SELECT in
 * State::SelectMode, indicating that the parameter mode should be exited.
 *
 * @return true  if parameter mode requests to exit completely,
 * @return false otherwise.
 */
bool ParametersMode::step() {
    Key s    = keys_.stable();
    Key prev = lastKey_;

    if (firstStep_) {
        if (s == Key::SELECT) {
            // discard the SELECT used to enter parameter mode
            keys_.clear();
            s = Key::NONE;
        }
        firstStep_ = false;
    }
    
    unsigned long now  = millis();
    bool longPress     = checkLongPress(s, now);

    bool keyChanged = (s != prev);
    lastKey_ = s;
    

    switch (state_) {

    case State::SelectMode:
        if (needRedraw_) {
            drawSelectMode();
            needRedraw_ = false;
        }
    
        if (keyChanged) {
            // any directional key toggles between MOD1 and MOD2
            if (s == Key::UP || s == Key::DOWN || s == Key::LEFT || s == Key::RIGHT) {
                selectedMode_ = (selectedMode_ == 0 ? 1 : 0);
                needRedraw_ = true;
            }
    
            // short SELECT release (prev SELECT, current NONE) enters parameter list
            // unless it came from a long-press event (ignoreSelectRelease_ == true)
            if (prev == Key::SELECT && s == Key::NONE && !longPress) {
                if (ignoreSelectRelease_) {
                    ignoreSelectRelease_ = false;
                } else {
                    selectedParam_ = 0;
                    state_      = State::SelectParam;
                    needRedraw_ = true;
                }
            }
        }
    
        // long SELECT in mode-selection state: exit parameter mode
        if (longPress) {
            return true;
        }
        break;

    case State::SelectParam:
        if (needRedraw_) {
            drawSelectParam();
            needRedraw_ = false;
        }

        if (keyChanged) {
            if (s == Key::UP) {
                if (selectedParam_ > 0) {
                    selectedParam_--;
                    needRedraw_ = true;
                }
            }
            if (s == Key::DOWN) {
                int maxIdx = paramCountForMode(selectedMode_) - 1;
                if (selectedParam_ < maxIdx) {
                    selectedParam_++;
                    needRedraw_ = true;
                }
            }

            if (s == Key::SELECT && !longPress) {
                // decide whether to enter float or int editor
                blinkTs_    = millis();
                blinkBlock_ = false;
                
                if (selectedMode_ == 0) {
                    // MOD1: all parameters are float
                    float v =
                        (selectedParam_ == 0) ? gParams.mod1.plungeAfterSurface_mm :
                        (selectedParam_ == 1) ? gParams.mod1.etchingThreshold_A :
                                                gParams.mod1.retractSpeed_mm_s;
                    startEditFloat(v);
                    state_ = State::EditFloat;
                } else {
                    // MOD2
                    if (selectedParam_ == 3) {
                        startEditInt(gParams.mod2.pulseCount);
                        state_ = State::EditInt;
                    } else {
                        float v =
                            (selectedParam_ == 0) ? gParams.mod2.plungeAfterSurface_mm :
                            (selectedParam_ == 1) ? gParams.mod2.etchingThreshold_A :
                            (selectedParam_ == 2) ? gParams.mod2.plungeAfterEtch_mm :
                            (selectedParam_ == 4) ? gParams.mod2.pulseOn_s : 0.0f;
                        startEditFloat(v);
                        state_ = State::EditFloat;
                    }
                }
            }
        }

        if (longPress) {
            // long SELECT: go back to mode selection
            state_      = State::SelectMode;
            needRedraw_ = true;
            ignoreSelectRelease_ = true;
        }
        break;

    case State::EditFloat:
        // on first entry, just draw the current value
        if (needRedraw_) {
            updateFloatEditor(Key::NONE);
            needRedraw_ = false;
        }

        if (keyChanged) {
            if (s == Key::UP || s == Key::DOWN || s == Key::LEFT || s == Key::RIGHT) {
                updateFloatEditor(s);
                blinkTs_    = millis();
                blinkBlock_ = false;
            }

            // short SELECT: save and return to parameter selection
            if (s == Key::SELECT && !longPress) {
                float v = floatFromDigits();
                if (selectedMode_ == 0) {
                    if      (selectedParam_ == 0) gParams.mod1.plungeAfterSurface_mm = v;
                    else if (selectedParam_ == 1) gParams.mod1.etchingThreshold_A     = v;
                    else if (selectedParam_ == 2) gParams.mod1.retractSpeed_mm_s      = v;
                } else {
                    if      (selectedParam_ == 0) gParams.mod2.plungeAfterSurface_mm = v;
                    else if (selectedParam_ == 1) gParams.mod2.etchingThreshold_A    = v;
                    else if (selectedParam_ == 2) gParams.mod2.plungeAfterEtch_mm    = v;
                    else if (selectedParam_ == 4) gParams.mod2.pulseOn_s             = v;
                }
                state_      = State::SelectParam;
                needRedraw_ = true;
            }
        }

        if (longPress) {
            // long SELECT: save and return to mode selection
            float v = floatFromDigits();
            if (selectedMode_ == 0) {
                if      (selectedParam_ == 0) gParams.mod1.plungeAfterSurface_mm = v;
                else if (selectedParam_ == 1) gParams.mod1.etchingThreshold_A     = v;
                else if (selectedParam_ == 2) gParams.mod1.retractSpeed_mm_s      = v;
            } else {
                if      (selectedParam_ == 0) gParams.mod2.plungeAfterSurface_mm = v;
                else if (selectedParam_ == 1) gParams.mod2.etchingThreshold_A    = v;
                else if (selectedParam_ == 2) gParams.mod2.plungeAfterEtch_mm    = v;
                else if (selectedParam_ == 4) gParams.mod2.pulseOn_s             = v;
            }
            state_      = State::SelectMode;
            needRedraw_ = true;
            ignoreSelectRelease_ = true;
        }

        // cursor blinking (float editor)
        if (state_ == State::EditFloat) {
            unsigned long dt = millis() - blinkTs_;
    
            if (!blinkBlock_ && dt >= 1000UL) {
                blinkBlock_ = true;
                blinkTs_    = millis();
    
                lcd_.setCursor(cursor_, 1);
                lcd_.write((char)255);
            }
            else if (blinkBlock_ && dt >= 200UL) {
                blinkBlock_ = false;
                blinkTs_    = millis();
    
                lcd_.setCursor(cursor_, 1);
                lcd_.write(digits_[cursor_]);
            }
        }
        break;

    case State::EditInt:
        // first entry: draw current value
        if (needRedraw_) {
            updateIntEditor(Key::NONE);
            needRedraw_ = false;
            blinkTs_    = millis();
            blinkBlock_ = false;
        }
    
        if (keyChanged) {
            if (s == Key::UP || s == Key::DOWN || s == Key::LEFT || s == Key::RIGHT) {
                updateIntEditor(s);
                blinkTs_    = millis();
                blinkBlock_ = false;
            }
    
            if (s == Key::SELECT && !longPress) {
                int v = intFromDigits();
                if (selectedMode_ == 1 && selectedParam_ == 3) {
                    gParams.mod2.pulseCount = v;
                }
                state_      = State::SelectParam;
                needRedraw_ = true;
            }
        }
    
        if (longPress) {
            int v = intFromDigits();
            if (selectedMode_ == 1 && selectedParam_ == 3) {
                gParams.mod2.pulseCount = v;
            }
            state_      = State::SelectMode;
            needRedraw_ = true;
            ignoreSelectRelease_ = true;
        }
    
        // cursor blinking (integer editor)
        {
            unsigned long dt = millis() - blinkTs_;
    
            if (!blinkBlock_ && dt >= 2000UL) {
                blinkBlock_ = true;
                blinkTs_    = millis();
    
                lcd_.setCursor(icursor_, 1);
                lcd_.write((char)255);
            }
            else if (blinkBlock_ && dt >= 200UL) {
                blinkBlock_ = false;
                blinkTs_    = millis();
    
                lcd_.setCursor(icursor_, 1);
                lcd_.write(idigits_[icursor_]);
            }
        }
    
        break;

    }

    return false;
}
