#include "Parameters.h"

/**
 * @file Parameters.cpp
 * @brief Definition of global default processing parameters for MOD1 and MOD2.
 *
 * This file contains the initialization of the global AllParams instance
 * `gParams`, which provides configurable processing parameters for the MOD1 and
 * MOD2 operating modes. These parameters include mechanical motions (plunge,
 * retract distances), current thresholds, timing settings, and pulse-generation
 * parameters used during surface detection, etching, or post-processing steps.
 *
 * The values defined here represent the default configuration and may be
 * overridden or adjusted at runtime if the system supports parameter editing.
 */

/**
 * @brief Global parameter structure holding all configurable settings.
 *
 * Contains:
 *  - Parameters for MOD1:
 *      * plungeAfterSurface_mm : additional downward motion after surface detection.
 *      * etchingThreshold_A    : current threshold for ending the etching phase.
 *      * retractSpeed_mm_s     : upward speed during the etching phase.
 *
 *  - Parameters for MOD2:
 *      * plungeAfterSurface_mm : analogous plunge distance for MOD2.
 *      * etchingThreshold_A    : current threshold during the validation/etching stages.
 *      * plungeAfterEtch_mm    : extra downward motion after etching.
 *      * pulseCount            : number of 9 V pulses to generate.
 *      * pulseOn_ms            : ON duration of each 9 V pulse.
 *      * pulseOff_ms           : OFF duration between 9 V pulses.
 */
AllParams gParams = {
    // --- MOD1 ---
    {
        4.0f,      ///< plungeAfterSurface_mm — downward distance after surface detection
        0.05f,     ///< etchingThreshold_A — current threshold for ending MOD1 etching
        0.015f     ///< retractSpeed_mm_s — upward etching speed
    },
    // --- MOD2 ---
    {
        4.0f,      ///< plungeAfterSurface_mm — downward distance after surface detection
        0.05f,     ///< etchingThreshold_A — current threshold during validation/etching
        3.0f,      ///< plungeAfterEtch_mm — additional downward travel after etching
        5,         ///< pulseCount — number of 9 V pulses to perform
        0.5f,      ///< pulseOn_ms — duration of pulse ON (in seconds)
        2.0f       ///< pulseOff_ms — duration of pulse OFF (in seconds)
    }
};
