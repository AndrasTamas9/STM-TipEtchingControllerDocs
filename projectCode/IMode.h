#pragma once
#include <Arduino.h>

/**
 * @brief Interface for non-blocking machine modes.
 *
 * This interface represents a generic operational mode of a system or device.
 * Implementations define:
 *  - a human-readable name,
 *  - an initialization routine executed once at mode entry,
 *  - a repeatedly called step function for non-blocking operation,
 *  - a cleanup function executed on exit.
 *
 * The step() method must be designed to run quickly without blocking; it is
 * called in a loop until it returns true, which signals that the mode is
 * complete and control can return to a menu or higher-level controller.
 */
struct IMode {

    /**
     * @brief Virtual destructor for proper cleanup via base pointers.
     */
    virtual ~IMode() {}

    /**
     * @brief Get the human-readable name of the mode.
     *
     * @return Null-terminated C-string containing the mode name.
     */
    virtual const char* name() const = 0;

    /**
     * @brief Initialize the mode.
     *
     * Called exactly once when the mode is entered. Implementations should
     * prepare internal state, peripherals, or resources needed during the mode.
     */
    virtual void begin() = 0;

    /**
     * @brief Perform one non-blocking operational step.
     *
     * This method is called repeatedly in the main control loop. The function
     * should avoid blocking delays and perform only incremental work. When it
     * returns true, the mode signals completion and the controller may exit the
     * mode and transition elsewhere.
     *
     * @return true  if the mode has finished and should exit,
     * @return false if it should be called again on the next loop iteration.
     */
    virtual bool step() = 0;

    /**
     * @brief Cleanup performed when the mode is exited.
     *
     * Called once at mode termination. Implementations should release resources,
     * reset hardware configurations, or leave the system in a well-defined state.
     */
    virtual void end() = 0;
};
