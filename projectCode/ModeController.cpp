#include "ModeController.h"

/**
 * @file ModeController.cpp
 * @brief Implementation of the ModeController class for LCD + keypad-driven mode selection.
 *
 * ModeController provides a simple user interface to:
 *  - browse a set of IMode implementations using LEFT/RIGHT keys,
 *  - start a selected mode with SELECT,
 *  - run the active mode in a non-blocking fashion,
 *  - optionally exit a running mode with SELECT (for non-"JOG"/"PARAM" modes).
 *
 * It uses an Lcd1602 instance for display output and a KeypadShield for key input.
 */

/**
 * @brief Construct a new ModeController.
 *
 * @param lcd        Reference to an Lcd1602 display used for the menu and status.
 * @param keys       Reference to a KeypadShield used for user input.
 * @param modes      Array of pointers to IMode objects managed by this controller.
 * @param modeCount  Number of entries in the modes array.
 */
ModeController::ModeController(Lcd1602& lcd, KeypadShield& keys, IMode** modes, uint8_t modeCount)
  : lcd_(lcd), keys_(keys), modes_(modes), n_(modeCount) {}

/**
 * @brief Initialize the controller and start the default mode.
 *
 * This method:
 *  - resets the selected menu index to zero,
 *  - switches the UI state to the MENU,
 *  - draws the initial menu screen,
 *  - automatically starts mode index 0 as the default ("home") mode.
 */
void ModeController::begin(){
  selected_=0;
  ui_=UiState::MENU;
  drawMenu_();
  start_(0);
}

/**
 * @brief Render the mode selection menu on the LCD.
 *
 * The menu has the following layout:
 *  - Line 0: static title "Select Mode:"
 *  - Line 1: "< <mode_name> >" centered within 16 characters.
 *
 * The currently selected mode is taken from modes_[selected_].
 */
void ModeController::drawMenu_(){
  lcd_.clear();
  lcd_.setCursor(0,0); lcd_.print(F("Select Mode:"));
  lcd_.setCursor(0,1); lcd_.print("< "); lcd_.print(modes_[selected_]->name());
  int used = 2 + strlen(modes_[selected_]->name());
  for(int i=0;i<16-used-2;++i) lcd_.print(" ");
  lcd_.print(" >");
}

/**
 * @brief Start a mode by index and switch UI state to RUNNING.
 *
 * The mode's begin() method is called once before the first step() invocation.
 *
 * @param idx  Index of the mode to start (0 <= idx < n_).
 */
void ModeController::start_(uint8_t idx){
  running_=idx;
  modes_[running_]->begin();
  ui_=UiState::RUNNING;
}

/**
 * @brief Stop the currently running mode and return to the menu.
 *
 * Calls end() on the active mode, sets the UI state back to MENU, and redraws
 * the menu so the user can select a new mode.
 */
void ModeController::stop_(){
  modes_[running_]->end();
  ui_=UiState::MENU;
  drawMenu_();
}

/**
 * @brief Main controller loop to be called periodically from the Arduino loop().
 *
 * Behavior:
 *  - Reads the next key event from the keypad.
 *  - If in MENU state:
 *      - LEFT  : select previous mode (with wrap-around) and redraw menu.
 *      - RIGHT : select next mode (with wrap-around) and redraw menu.
 *      - SELECT: start the currently selected mode and switch to RUNNING.
 *  - If in RUNNING state:
 *      - Calls step() on the active mode.
 *      - Determines whether the mode is a "JOG" or "PARAM" mode based on its
 *        name; for such modes, SELECT does not trigger a global exit.
 *      - For all other modes, pressing SELECT stops the mode and returns to the
 *        menu.
 *      - If step() returns true, the mode indicates completion and is stopped
 *        automatically.
 */
void ModeController::loop(){
  Key k = keys_.poll();
  if(ui_==UiState::MENU){
    if(k==Key::LEFT){ 
      selected_ = (selected_==0? n_-1 : selected_-1); 
      drawMenu_(); 
    }
    else if(k==Key::RIGHT){ 
      selected_ = (selected_+1)%n_; 
      drawMenu_(); 
    }
    else if(k==Key::SELECT){
      start_(selected_); 
    }
  } else { // RUNNING
    bool done = modes_[running_]->step();

    // Determine if this is a JOG-like mode in which SELECT should not cause a global exit.
    const char* rn = modes_[running_]->name();
    bool isJog =
             (strcmp(rn, "JOG")  == 0) ||
             (strcmp(rn, "PARAM") == 0);

    // For non-JOG modes, SELECT acts as a global "exit to menu".
    if (!isJog && k == Key::SELECT) {
      stop_();
      return;
    }

    // If the active mode reports completion, stop it and go back to the menu.
    if (done) {
      stop_();
    }
  }
}
