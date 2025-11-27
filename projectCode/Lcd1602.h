#pragma once
#include <Arduino.h>
#include <LiquidCrystal.h>

/**
 * @brief Wrapper class for a 16x2 HD44780-compatible LCD with PWM backlight control.
 *
 * This class encapsulates a LiquidCrystal instance and adds:
 *  - convenient print overloads,
 *  - PWM-based backlight brightness control,
 *  - an optional inversion mode for shields with reversed backlight polarity,
 *  - helper formatting utilities such as title2().
 *
 * It is designed as a drop-in usability enhancement over the standard
 * LiquidCrystal API.
 */
class Lcd1602 {
  
public:
  /**
   * @brief Construct a new Lcd1602 object.
   *
   * @param rs            LCD RS pin.
   * @param en            LCD Enable pin.
   * @param d4            LCD data line D4.
   * @param d5            LCD data line D5.
   * @param d6            LCD data line D6.
   * @param d7            LCD data line D7.
   * @param backlightPin  PWM-capable output pin controlling the backlight.
   * @param invertBL      If true, invert the PWM duty cycle (useful for boards
   *                      where backlight wiring is inverted by default).
   */
  Lcd1602(uint8_t rs,uint8_t en,uint8_t d4,uint8_t d5,uint8_t d6,uint8_t d7,
          uint8_t backlightPin, bool invertBL=false);

  /**
   * @brief Initialize the LCD hardware and backlight subsystem.
   *
   * Configures the backlight pin as output, applies the stored brightness
   * setting, and initializes the LiquidCrystal driver for a 16×2 display.
   */
  void begin();

  /**
   * @brief Clear the LCD display.
   */
  void clear();

  /**
   * @brief Position the cursor.
   *
   * @param c Column index (0–15).
   * @param r Row index (0–1).
   */
  void setCursor(uint8_t c,uint8_t r);

  /**
   * @brief Print a RAM-resident C-string.
   *
   * @param s Null-terminated character string.
   */
  void print(const char* s);

  /**
   * @brief Print a flash-resident string (F("...")).
   *
   * @param s Pointer to flash-resident string.
   */
  void print(const __FlashStringHelper* s);

  /**
   * @brief Print a signed integer.
   */
  void print(int v);

  /**
   * @brief Print an unsigned 8-bit integer.
   */
  void print(uint8_t v);

  /**
   * @brief Print a signed long integer.
   */
  void print(long v);

  /**
   * @brief Print an unsigned long integer.
   */
  void print(unsigned long v);

  /**
   * @brief Print a floating-point value with a given precision.
   *
   * @param v    Floating-point number.
   * @param prec Number of decimal digits.
   */
  void print(float v, uint8_t prec);

  /**
   * @brief Write a single raw character to the display.
   *
   * @param ch Character to write.
   */
  void write(char ch);

  /**
   * @brief Set the backlight brightness using PWM.
   *
   * @param pwm Brightness level (0–255).
   */
  void setBacklight(uint8_t pwm);

  /**
   * @brief Get the currently stored backlight brightness (not inverted).
   *
   * @return PWM value (0–255).
   */
  uint8_t backlight() const { return blVal_; }

  /**
   * @brief Display two lines of flash-resident text as a title.
   *
   * Clears the screen, then prints the provided strings on line 0 and line 1.
   *
   * @param l1 First line text (flash-resident).
   * @param l2 Second line text (flash-resident).
   */
  void Lcd1602::title2(const __FlashStringHelper* l1, const __FlashStringHelper* l2);

private:
  /** @brief Underlying LiquidCrystal driver instance. */
  LiquidCrystal lcd_;

  /** @brief PWM pin controlling the LCD backlight. */
  uint8_t blPin_;

  /** @brief Whether PWM duty cycle should be inverted. */
  bool inv_;

  /** @brief Stored backlight brightness level (0–255). */
  uint8_t blVal_ = 200;
};
