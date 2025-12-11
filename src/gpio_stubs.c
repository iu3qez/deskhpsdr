/*
 * GPIO hardware support stubs for Windows
 *
 * This file provides empty stub definitions for GPIO variables and functions
 * that are defined in gpio.c (Linux-only) but referenced by other parts of the code.
 *
 * FIXME: TODO - GPIO support not available on Windows
 * See TODO.md for details on GPIO hardware support
 */

#ifdef _WIN32

#include <gtk/gtk.h>  // For gboolean, guchar, gulong types
#include <stddef.h>   // For NULL
#include "gpio.h"

// Provide stub definitions for GPIO global variables
// These are normally defined in gpio.c which is excluded from Windows builds

SWITCH switches_controller1[MAX_FUNCTIONS][MAX_SWITCHES] = {{{0}}};
SWITCH *switches = NULL;
ENCODER *encoders = NULL;  // encoders is a pointer, not an array

// Provide stub implementations for GPIO functions
// These are normally defined in gpio.c which is excluded from Windows builds

void gpio_set_defaults(int ctrlr) {
    (void)ctrlr;
    // GPIO hardware not supported on Windows
}

void gpio_default_encoder_actions(int ctrlr) {
    (void)ctrlr;
    // GPIO hardware not supported on Windows
}

void gpio_default_switch_actions(int ctrlr) {
    (void)ctrlr;
    // GPIO hardware not supported on Windows
}

void gpioSaveState(void) {
    // GPIO hardware not supported on Windows
}

void gpioRestoreState(void) {
    // GPIO hardware not supported on Windows
}

void gpioSaveActions(void) {
    // GPIO hardware not supported on Windows
}

void gpioRestoreActions(void) {
    // GPIO hardware not supported on Windows
}

#endif /* _WIN32 */
