#include "Lcd1602.h"

/**
 * @file Lcd1602.cpp
 * @brief Implementation of a wrapper class for a 16x2 HD44780-compatible LCD with PWM backlight control.
 *
 * This class extends the standard LiquidCrystal interface by:
 *  - providing unified print/write forwarding,
 *  - adding software-controlled PWM backlight brightness,
 *  - offering convenience drawing routines such as title2().
 *
 * It does not alter the behavior of the underlying LiquidCrystal object, only
 * wraps it to simplify usage and centralize LCD-related utilities.
 */

/**
 * @brief Construct a new Lcd1602 object.
 *
 * @param rs            Register Select pin for the LCD.
 * @param en            Enable pin for the LCD.
 * @param d4            Data line D4.
 * @param d5            Data line D5.
 * @param d6            Data line D6.
 * @param d7            Data line D7.
 * @param backlightPin  PWM-capable pin used to control the backlight.
 * @param invertBL      If true, the PWM duty cycle is inverted (useful for shields
 *                      where the transistor wiring reverses the backlight control logic).
 */
Lcd1602::Lcd1602(uint8_t rs,uint8_t en,uint8_t d4,uint8_t d5,uint8_t d6,uint8_t d7,
                 uint8_t backlightPin, bool invertBL)
  : lcd_(rs,en,d4,d5,d6,d7), blPin_(backlightPin), inv_(invertBL) {}

/**
 * @brief Initialize the LCD hardware and set the initial backlight state.
 *
 * This function configures the backlight pin as output, applies the stored
 * backlight PWM value, and initializes the underlying LiquidCrystal instance
 * for a 16×2 display.
 */
void Lcd1602::begin(){
  pinMode(blPin_, OUTPUT);
  setBacklight(blVal_);
  lcd_.begin(16,2);
}

/**
 * @brief Clear the LCD screen.
 */
void Lcd1602::clear(){ lcd_.clear(); }

/**
 * @brief Set the cursor position on the LCD.
 *
 * @param c  Column index (0–15).
 * @param r  Row index (0–1).
 */
void Lcd1602::setCursor(uint8_t c,uint8_t r){ lcd_.setCursor(c,r); }

/**
 * @brief Print a RAM-resident C-string to the display.
 *
 * @param s  Null-terminated character string.
 */
void Lcd1602::print(const char* s){ lcd_.print(s); }

/**
 * @brief Print a PROGMEM (flash-resident) string to the display.
 *
 * @param s  Flash string wrapped in __FlashStringHelper.
 */
void Lcd1602::print(const __FlashStringHelper* s){ lcd_.print(s); }

/**
 * @brief Overload: print a signed integer.
 */
void Lcd1602::print(int v){ lcd_.print(v); }

/**
 * @brief Overload: print an unsigned 8-bit integer.
 */
void Lcd1602::print(uint8_t v){ lcd_.print(v); }

/**
 * @brief Overload: print a signed long integer.
 */
void Lcd1602::print(long v){ lcd_.print(v); }

/**
 * @brief Overload: print an unsigned long integer.
 */
void Lcd1602::print(unsigned long v){ lcd_.print(v); }

/**
 * @brief Overload: print a floating-point number with precision.
 *
 * @param v     Floating-point value.
 * @param prec  Number of digits after the decimal point.
 */
void Lcd1602::print(float v, uint8_t prec) { lcd_.print(v, prec); }

/**
 * @brief Write a single raw character to the display.
 *
 * @param ch  Character to write.
 */
void Lcd1602::write(char ch){ lcd_.write(ch); }

/**
 * @brief Set the LCD backlight brightness using PWM.
 *
 * The backlight value is stored internally. If invertBL was set in the
 * constructor, the PWM duty cycle is inverted (pwm = 255 - pwm) before writing.
 *
 * @param pwm  PWM duty cycle (0–255) for backlight brightness.
 */
void Lcd1602::setBacklight(uint8_t pwm){
  blVal_ = pwm;
  if(inv_) pwm = 255 - pwm;
  analogWrite(blPin_, pwm);
}

/**
 * @brief Display a two-line title, clearing the screen before writing.
 *
 * This convenience function clears the LCD, places the cursor at the beginning
 * of line 0 and line 1, and writes two provided flash-resident strings.
 *
 * @param l1  First line text (flash-resident).
 * @param l2  Second line text (flash-resident).
 */
void Lcd1602::title2(const __FlashStringHelper* l1,
                     const __FlashStringHelper* l2)
{
    lcd_.clear();
    lcd_.setCursor(0,0);
    lcd_.print(l1);
    lcd_.setCursor(0,1);
    lcd_.print(l2);
}
