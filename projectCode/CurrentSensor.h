// CurrentSensor.h
#pragma once
#include <Arduino.h>

/**
 * @brief Baseline offset current used to correct measured RMS current.
 *
 * This global variable represents the sensor's residual or idle current reading
 * when no real load is present. It is subtracted from the internally computed
 * RMS current in correctedIrms() to compensate for sensor noise and offsets.
 */
extern float baselineCurrent;

/**
 * @brief Non-blocking RMS current measurement helper class.
 *
 * The CurrentSensor class performs window-based RMS current measurement from an
 * analog input pin connected to a current sensor (for example, a current
 * transformer with burden resistor). It:
 *  - periodically samples the ADC at a configured interval,
 *  - accumulates statistics (min, max, sum, sum of squares) over a time window,
 *  - computes both peak-to-peak voltage and true AC RMS current at the end of
 *    each window.
 *
 * The measurement is non-blocking: update() should be called frequently in the
 * main loop, and the computations are spread over time according to the
 * sampling interval and window length.
 */
class CurrentSensor {
public:
  /**
   * @brief Construct a new CurrentSensor object with configuration parameters.
   *
   * @param analogPin          Analog input pin used to read the current sensor.
   * @param Vref               ADC reference voltage in volts (default 5.0 V).
   * @param adcMax             Maximum ADC reading (e.g. 1023.0 for 10-bit ADC).
   * @param k_cal              Calibration factor converting RMS voltage to RMS
   *                           current (includes burden resistor and CT ratio).
   * @param sampleWindow_us    Duration of the RMS integration window in
   *                           microseconds (default ~1 period at 50 Hz).
   * @param sampleInterval_us  Time between consecutive ADC samples in
   *                           microseconds (default ~5 kHz sampling rate).
   */
  CurrentSensor(uint8_t analogPin,
                float Vref = 5.0f,
                float adcMax = 1023.0f,
                float k_cal = 0.90f,
                unsigned long sampleWindow_us = 20000UL,   // ~1 period at 50 Hz
                unsigned long sampleInterval_us = 200UL);  // ~5 kHz

  /**
   * @brief Initialize the sensor and reset internal statistics.
   *
   * This method should be called once (typically from setup()). It configures
   * the pin mode for the analog input and resets timing and accumulated values
   * used for RMS calculations.
   */
  void begin();

  /**
   * @brief Perform one non-blocking update step of the measurement.
   *
   * This function should be called frequently from the main loop. It:
   *  - takes a new ADC sample when the sampling interval has elapsed,
   *  - updates min/max and RMS statistics for the current window,
   *  - computes Irms and Vpp at the end of each completed window.
   *
   * If the sensor is disabled via setEnabled(false), this function returns
   * immediately and does not update the last computed measurements.
   */
  void update();

  /**
   * @brief Get the last computed peak-to-peak voltage.
   *
   * This value corresponds to the peak-to-peak voltage of the sensor signal
   * over the most recently completed integration window.
   *
   * @return Last computed peak-to-peak voltage in volts.
   */
  float lastVpp() const  { return Vpp_; }

  /**
   * @brief Get the last computed RMS current.
   *
   * This is the raw RMS current derived from the AC component of the measured
   * voltage and scaled by the calibration factor k_cal. It does not include any
   * baseline offset correction.
   *
   * @return Last computed RMS current in amperes.
   */
  float lastIrms() const { return Irms_; }

  /**
   * @brief Get the baseline-corrected RMS current.
   *
   * The function subtracts the global baselineCurrent from the internally
   * computed RMS current and clamps the result to zero if it would become
   * negative. This is useful to eliminate sensor noise when no load is present.
   *
   * @return Baseline-corrected RMS current in amperes (non-negative).
   */
  float correctedIrms() const;
  
  /**
   * @brief Enable or disable measurement updates.
   *
   * When disabled, calls to update() do nothing and the last computed Irms and
   * Vpp values are preserved.
   *
   * @param en True to enable measurements, false to disable them.
   */
  void setEnabled(bool en) { enabled_ = en; }

  /**
   * @brief Check whether the sensor is currently enabled.
   *
   * @return True if measurement updates are enabled, false otherwise.
   */
  bool isEnabled() const { return enabled_; }

private:
  /** @brief Analog input pin used for sensor reading. */
  uint8_t pin_;

  /** @brief ADC reference voltage in volts. */
  float   Vref_;

  /** @brief Maximum ADC reading (e.g. 1023.0 for 10-bit ADC). */
  float   adcMaxValue_;

  /** @brief Calibration factor converting RMS voltage to RMS current. */
  float   k_cal_;

  /** @brief Flag indicating whether measurements are currently enabled. */
  bool enabled_ = false;

  /** @brief Integration window duration in microseconds. */
  unsigned long sampleWindow_us_;

  /** @brief Sampling interval in microseconds between ADC reads. */
  unsigned long sampleInterval_us_;

  /** @brief Timestamp of the next scheduled ADC sample (in microseconds). */
  unsigned long nextSampleTime_ = 0;

  /** @brief Start timestamp of the current integration window (in microseconds). */
  unsigned long windowStart_    = 0;

  /** @brief Minimum ADC value observed in the current window. */
  int adcMin_ = 1023;

  /** @brief Maximum ADC value observed in the current window. */
  int adcMax_ = 0;

  /** @brief Last computed peak-to-peak voltage in volts. */
  float Vpp_  = 0.0f;

  /** @brief Last computed RMS current in amperes (uncorrected). */
  float Irms_ = 0.0f;

  /**
   * @brief Accumulator of voltage samples Σ v for RMS computation.
   *
   * Used together with sumV2_ and nSamples_ to derive the mean and variance of
   * the measured signal within a window, allowing computation of the AC RMS
   * voltage after removing the DC offset.
   */
  float    sumV_      = 0.0f;   // Σ v

  /**
   * @brief Accumulator of squared voltage samples Σ v^2 for RMS computation.
   */
  float    sumV2_     = 0.0f;   // Σ v^2

  /**
   * @brief Number of samples collected in the current window.
   */
  uint32_t nSamples_  = 0;      // N
};
