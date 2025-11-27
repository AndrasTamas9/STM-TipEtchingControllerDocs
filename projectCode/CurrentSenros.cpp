/**
 * @file CurrentSensor.cpp
 * @brief Implementation of the CurrentSensor class for RMS current measurement.
 *
 * This module provides a non-blocking, window-based RMS current measurement using
 * periodic ADC sampling. A sliding (time-based) window is used to track:
 *  - the minimum and maximum ADC values within the window (for peak-to-peak voltage),
 *  - the first and second moment of the measured voltage signal (for true AC RMS).
 *
 * The class expects:
 *  - an analog input pin connected to a current sensor (e.g. CT sensor with burden resistor),
 *  - the ADC reference voltage and maximum ADC code,
 *  - a calibration constant that converts measured RMS voltage to RMS current.
 */

#include "CurrentSensor.h"

/**
 * @brief Constructs a new CurrentSensor instance.
 *
 * The constructor only stores configuration parameters; it does not configure
 * the hardware pin or reset internal statistics. Call begin() once before
 * starting periodic measurements.
 *
 * @param analogPin          The analog input pin number used for current sensing.
 * @param Vref               The ADC reference voltage in volts.
 * @param adcMaxValue        The maximum ADC reading (e.g. 1023.0 for 10-bit ADC).
 * @param k_cal              Calibration factor to convert RMS voltage to RMS current.
 *                           Typically this includes the burden resistor value and
 *                           current transformer ratio.
 * @param sampleWindow_us    Duration of the RMS integration window in microseconds.
 * @param sampleInterval_us  Desired sampling interval in microseconds. The update()
 *                           method will attempt to sample at this rate based on micros().
 */
CurrentSensor::CurrentSensor(uint8_t analogPin,
                             float Vref,
                             float adcMaxValue,
                             float k_cal,
                             unsigned long sampleWindow_us,
                             unsigned long sampleInterval_us)
  : pin_(analogPin),
    Vref_(Vref),
    adcMaxValue_(adcMaxValue),
    k_cal_(k_cal),
    sampleWindow_us_(sampleWindow_us),
    sampleInterval_us_(sampleInterval_us)
{}

/**
 * @brief Initializes the sensor state and internal statistics.
 *
 * This method must be called once (typically from setup()) before calling update().
 * It configures the analog input pin, initializes the timing for the sampling
 * window, and resets all statistics used for RMS calculations.
 */
void CurrentSensor::begin() {
  pinMode(pin_, INPUT);
  unsigned long now = micros();
  windowStart_    = now;
  nextSampleTime_ = now;
  adcMin_ = 1023;
  adcMax_ = 0;

  // Reset RMS statistics for the current window.
  sumV_ = 0.0f;
  sumV2_ = 0.0f;
  nSamples_ = 0;
}

/**
 * @brief Performs one non-blocking update step of the RMS measurement.
 *
 * This function should be called frequently from the main loop. It:
 *  - takes a new ADC sample when the sampling interval has elapsed,
 *  - accumulates statistics (min, max, sum, sum of squares),
 *  - closes and advances the integration window when the window duration expires,
 *  - computes the AC RMS current (Irms_) at the end of each window.
 *
 * Behavior details:
 *  - If the sensor is disabled (enabled_ == false), the function returns immediately
 *    and does not modify the last computed Irms_ value.
 *  - Sampling is driven by micros() and integer arithmetic to be robust against
 *    micros() overflow.
 *  - The AC RMS is computed as sqrt( <v^2> - <v>^2 ), i.e. the RMS of the
 *    AC component after removing DC offset, and then scaled by k_cal_.
 */
void CurrentSensor::update() {
  if (!enabled_) {
      // When disabled, do not update any statistics or timing; keep lastIrms() unchanged.
      return;
  }
  unsigned long now = micros();

  // Time-based sampling: take a new sample when now reaches or exceeds nextSampleTime_.
  // Cast to int32_t to safely handle micros() overflow when computing differences.
  if ((int32_t)(now - nextSampleTime_) >= 0) {
    nextSampleTime_ += sampleInterval_us_;
    int adc = analogRead(pin_);
    //adc = 750;

    if (adc < adcMin_) adcMin_ = adc;
    if (adc > adcMax_) adcMax_ = adc;

    // Convert ADC reading to voltage (including DC bias) and accumulate RMS statistics.
    float v = adc * (Vref_ / adcMaxValue_);  // Voltage in volts, bias included.
    sumV_  += v;
    sumV2_ += v * v;
    nSamples_++;
  }

  // Check if the current integration window has elapsed.
  if ((int32_t)(now - windowStart_) >= (int32_t)sampleWindow_us_) {
    windowStart_ += sampleWindow_us_;

    // Compute peak-to-peak span and corresponding voltage.
    int span = adcMax_ - adcMin_;
    if (span < 0) span = 0; // Safety guard, should not normally happen.

    Vpp_  = span * (Vref_ / adcMaxValue_);

    // Compute true AC RMS voltage based on the accumulated statistics.
    if (nSamples_ > 0) {
      float meanV  = sumV_  / (float)nSamples_;  // <v>
      float meanV2 = sumV2_ / (float)nSamples_;  // <v^2>

      // Variance of the signal (AC component squared average): <v^2> - <v>^2
      float var = meanV2 - meanV * meanV;
      if (var < 0.0f) var = 0.0f;                // Numerical guard against negative values.

      float Vrms_ac = sqrtf(var);                // AC RMS voltage (DC offset removed).
      Irms_ = k_cal_ * Vrms_ac;                  // Calibrated RMS current.
    }

    // Reset statistics for the next integration window.
    adcMin_ = 1023;
    adcMax_ = 0;

    sumV_ = 0.0f;
    sumV2_ = 0.0f;
    nSamples_ = 0;
  }
}

/**
 * @brief Returns the baseline-corrected RMS current.
 *
 * This function subtracts a baseline offset current (baselineCurrent) from the
 * internally computed RMS current (Irms_) and clamps the result to zero if the
 * subtraction would produce a negative value. This is useful to compensate for
 * sensor noise and offsets when no load is present.
 *
 * @return The corrected RMS current in amperes, guaranteed to be non-negative.
 */
float CurrentSensor::correctedIrms() const {
    float I = Irms_ - baselineCurrent;
    if (I < 0.0f) I = 0.0f; // Clamp negative results caused by noise or offsets.
    return I;
}
