#include "MovingAverage.h"
#include <Arduino.h>

/**
 * @file MovingAverage.cpp
 * @brief Implementation of a fixed-point, template-based moving average filter.
 *
 * This file provides the method definitions for the MovingAverage template,
 * which implements an efficient sliding window average using a fixed-size
 * circular buffer and integer (fixed-point) accumulation for improved numeric
 * stability on microcontrollers.
 *
 * The template parameters are:
 *  - N      : window length (number of samples),
 *  - SCALE  : fixed-point scaling factor applied to the input values.
 */

/**
 * @brief Default constructor.
 *
 * Initializes the internal buffer, accumulator, and state by calling reset()
 * with the default initial value (0.0f). After construction, the filter output
 * is zero until new samples are added via update().
 *
 * @tparam N     Window length (number of samples).
 * @tparam SCALE Fixed-point scaling factor used for internal representation.
 */
template<int N, int SCALE>
MovingAverage<N, SCALE>::MovingAverage() {
    reset();
}

/**
 * @brief Convert a floating-point value to fixed-point (int16_t) with scaling.
 *
 * The input value xA (in "analog units" or any chosen units) is multiplied by
 * SCALE and then clamped to the range representable by int16_t:
 *  - [-32768, 32767].
 *
 * This helps to keep the internal representation compact and efficient while
 * maintaining a controlled scaling factor.
 *
 * @param xA   Input value in floating point.
 * @return     Fixed-point representation of the input, scaled by SCALE.
 */
template<int N, int SCALE>
int16_t MovingAverage<N, SCALE>::toFixed_(float xA) {
    float xmA = xA * SCALE;
    if (xmA > 32767.0f) xmA = 32767.0f;
    if (xmA < -32768.0f) xmA = -32768.0f;
    return (int16_t)lroundf(xmA);
}

/**
 * @brief Add a new sample to the moving average and return the updated average.
 *
 * This method:
 *  - Converts the new sample to fixed-point and inserts it into the circular
 *    buffer at the current index.
 *  - Subtracts the previous value at that position from the running sum and
 *    adds the new sample.
 *  - Advances the circular index and, once the buffer has wrapped at least
 *    once, marks it as "filled".
 *  - Computes the average as (sum / denom) / SCALE, where:
 *      - denom is N when the buffer is filled,
 *      - otherwise denom is the number of actually inserted samples.
 *
 * The computation is designed to be O(1) per update and numerically stable for
 * typical microcontroller use cases.
 *
 * @param xA  New input sample in floating point.
 * @return    Current moving average value after including the new sample.
 */
template<int N, int SCALE>
float MovingAverage<N, SCALE>::update(float xA) {
    int16_t xmA = toFixed_(xA);

    sum_ -= buf_[idx_];
    buf_[idx_] = xmA;
    sum_ += xmA;

    idx_++;
    if (idx_ >= N) {
        idx_ = 0;
        filled_ = true;
    }

    int denom = filled_ ? N : idx_;
    if (denom <= 0) denom = 1;

    return (sum_ / (float)denom) / SCALE;
}

/**
 * @brief Reset the moving average filter to a predefined initial value.
 *
 * All N buffer entries are set to the same fixed-point representation of x0A,
 * and the accumulator sum_ is initialized accordingly. If x0A is non-zero,
 * the buffer is considered "filled" immediately; otherwise, filled_ is false
 * and the effective denominator grows as new samples are added.
 *
 * @param x0A  Initial value to pre-fill the window with (defaults to 0.0f if
 *             the caller uses reset() with no explicit argument in the header).
 */
template<int N, int SCALE>
void MovingAverage<N, SCALE>::reset(float x0A) {
    int16_t x0 = toFixed_(x0A);
    sum_ = 0;
    idx_ = 0;
    filled_ = false;

    for (int i = 0; i < N; i++) {
        buf_[i] = x0;
        sum_ += x0;
    }

    if (x0 != 0) filled_ = true;
}

/**
 * @brief Explicit template instantiations.
 *
 * These instantiations ensure that the compiler generates code for the specific
 * MovingAverage configurations that are used elsewhere in the project:
 *  - MovingAverage<20>   : short-window average (SCALE taken from header default).
 *  - MovingAverage<200>  : long-window average (SCALE taken from header default).
 *
 * If additional window sizes or SCALE values are required, add further explicit
 * instantiations here.
 */
template class MovingAverage<20>;
template class MovingAverage<200>;
