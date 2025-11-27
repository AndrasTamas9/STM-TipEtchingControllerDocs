#pragma once

/**
 * @brief Parameter set for MOD1 (surface detection + 30 V etching).
 *
 * These values define the operational behavior of MOD1, including:
 *  - the additional downward plunge after initial surface detection,
 *  - the current threshold at which etching is considered complete,
 *  - the upward retract speed during the etching phase.
 */
struct Mod1Params {
    float plungeAfterSurface_mm;   ///< Additional downward motion after detecting the surface (mm).
    float etchingThreshold_A;      ///< Current threshold for terminating the etching phase (A).
    float retractSpeed_mm_s;       ///< Upward retract speed during etching (mm/s).
};

/**
 * @brief Parameter set for MOD2 (surface detection + validation + 9 V pulses).
 *
 * These parameters control:
 *  - the plunge after detecting the surface,
 *  - the current threshold for ending the validation or etching-related stages,
 *  - the additional plunge after etching,
 *  - the number of 9 V pulses to apply,
 *  - the ON/OFF durations of each pulse in seconds.
 */
struct Mod2Params {
    float plungeAfterSurface_mm;   ///< Downward plunge after surface detection (mm).
    float etchingThreshold_A;      ///< Current threshold used in validation/etching phases (A).
    float plungeAfterEtch_mm;      ///< Downward motion after etching (mm).
    int   pulseCount;              ///< Number of 9 V pulses to apply.
    float pulseOn_s;               ///< ON duration of each 9 V pulse (seconds).
    float pulseOff_s;              ///< OFF duration between pulses (seconds).
};

/**
 * @brief Combined parameter structure containing all mode-specific settings.
 *
 * Holds one Mod1Params and one Mod2Params instance used for MOD1 and MOD2
 * operation. This makes it convenient to manage all runtime parameters through
 * a single global configuration object.
 */
struct AllParams {
    Mod1Params mod1;  ///< Parameters governing MOD1 behavior.
    Mod2Params mod2;  ///< Parameters governing MOD2 behavior.
};

/**
 * @brief Global parameter instance used throughout the system.
 *
 * Defined in Parameters.cpp. The values in this instance determine the default
 * behavior of MOD1 and MOD2 unless modified elsewhere in the program.
 */
extern AllParams gParams;
