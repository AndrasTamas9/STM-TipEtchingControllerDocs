#pragma once
#include <Arduino.h>

/**
 * @brief Fixed-point, static moving average filter.
 *
 * This class implements a compile-time sized, fixed-point moving average with:
 *  - no dynamic memory allocation (safe for small microcontrollers like AVR),
 *  - O(1) update cost using a circular buffer,
 *  - integer accumulation for improved numerical stability.
 *
 * Template parameters:
 *  @tparam N      Window size (number of samples in the average).
 *  @tparam SCALE  Fixed-point scaling factor applied to inputs (default = 1000),
 *                 allowing efficient integer accumulation. For example,
 *                 SCALE = 1000 means values are stored as "milliscale" units.
 *
 * The filter keeps a buffer of N int16_t values (scaled inputs) and an integer
 * accumulator (sum_) large enough to store the full sum without overflow.
 *
 * @note All operations are platform-friendly for typical Arduino boards.
 */
template<int N, int SCALE = 1000>
class MovingAverage {
public:
    /**
     * @brief Construct a new MovingAverage filter.
     *
     * Initializes the internal circular buffer and accumulator by calling
     * reset() with the default initial value (0.0f). After creation, the filter
     * outputs zero until new samples are added.
     */
    MovingAverage();

    /**
     * @brief Insert a new floating-point sample and compute the updated average.
     *
     * Steps performed:
     *  - Convert the input value to fixed-point using SCALE and clamping.
     *  - Remove the oldest sample from the accumulator and insert the new one.
     *  - Advance the circular index.
     *  - Compute the current average using the proper denominator:
     *      - N    when the buffer has been filled at least once,
     *      - idx_ when still filling for the first time.
     *
     * @param xA   New input sample in floating-point units.
     * @return     The current moving average as a floating-point value.
     */
    float update(float xA);

    /**
     * @brief Reset the filter contents and accumulator.
     *
     * Fills all N buffer entries with the fixed-point representation of x0A
     * and resets internal state. If x0A is non-zero, the buffer is considered
     * "filled"; otherwise the denominator will increase gradually until the
     * first wrap-around completes the buffer.
     *
     * @param x0A  Initial value to fill the entire window with (default = 0.0f).
     */
    void reset(float x0A = 0.0f);

    /**
     * @brief Check whether the moving average has been fully filled.
     *
     * @return true  if at least N samples have been inserted,
     * @return false otherwise.
     */
    bool filled() const { return filled_; }

private:
    /**
     * @brief Convert a floating-point input to a fixed-point int16_t sample.
     *
     * Applies SCALE, rounds to nearest integer, and clamps to the int16_t range
     * of [-32768, 32767]. This ensures safe accumulation in the internal buffer.
     *
     * @param xA  Input sample in floating-point representation.
     * @return    Fixed-point value suitable for storage and accumulation.
     */
    static int16_t toFixed_(float xA);

    /** @brief Circular buffer storing the last N scaled samples. */
    int16_t buf_[N];

    /** @brief Current index in the circular buffer (0..N-1). */
    int idx_ = 0;

    /** @brief Flag indicating that the buffer has wrapped once. */
    bool filled_ = false;

    /**
     * @brief Accumulated sum of the last N fixed-point samples.
     *
     * Stored as long to safely hold N * max(int16_t) even for typical N values.
     */
    long sum_ = 0;
};
