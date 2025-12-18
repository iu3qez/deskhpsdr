/* Copyright (C)
* 2025 - Windows/MinGW port for deskHPSDR
*
*   Stub implementations for functions not available/needed on Windows
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*/

#ifdef _WIN32

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include "../src/actions.h"

/*
 * GPIO STUBS
 * GPIO is Raspberry Pi specific - not available on Windows
 */

/* From gpio.h - encoder and switch structures */
typedef struct {
    int gpio_a;
    int gpio_b;
    int pos;
    int state;
    int bottom_encoder_enabled;
    int top_encoder_enabled;
    int top_encoder_address_a;
    int top_encoder_address_b;
    int switch_enabled;
    int switch_address;
    int bottom_encoder_pullup;
    int bottom_encoder_address_a;
    int bottom_encoder_address_b;
    int top_encoder_pullup;
    int switch_pullup;
    int encoder_is_int;
    int top_encoder_enabled_address;
    int bottom_encoder_enabled_address;
} ENCODER;

typedef struct {
    int switch_enabled;
    int switch_pullup;
    int switch_address;
    int switch_function;
    unsigned long switch_debounce;
} SWITCH;

/* Encoder array - used by encoder_menu.c */
ENCODER encoders[8] = {0};

/* Switch arrays - used by actions.c, toolbar.c, switch_menu.c */
SWITCH switches[16] = {0};
SWITCH switches_controller1[16] = {0};

void gpio_set_defaults(int ctrlr) {
    (void)ctrlr;
    /* No GPIO on Windows */
}

void gpioSaveState(void) {
    /* No GPIO on Windows */
}

void gpioRestoreState(void) {
    /* No GPIO on Windows */
}

void gpioSaveActions(void) {
    /* No GPIO on Windows */
}

void gpioRestoreActions(void) {
    /* No GPIO on Windows */
}

void gpio_default_encoder_actions(int ctrlr) {
    (void)ctrlr;
    /* No GPIO on Windows */
}

void gpio_default_switch_actions(int ctrlr) {
    (void)ctrlr;
    /* No GPIO on Windows */
}

/*
 * TOOLSET STUBS
 * toolset.c excluded because it requires libxml2 (via libsolar)
 */

/* Solar/space weather data - displayed on panadapter */
char solar_flux[32] = "N/A";
char a_index[32] = "N/A";
char k_index[32] = "N/A";
char xray[32] = "N/A";
char geomagfield[32] = "N/A";
char sunspots[32] = "N/A";

void toolset_init(void) {
    /* Toolset not available on Windows (requires libxml2) */
}

void check_and_run(void) {
    /* Toolset not available on Windows */
}

char *truncate_text_3p(const char *text, int max_len) {
    static char buffer[256];
    if (!text) return NULL;

    int len = strlen(text);
    if (len <= max_len) {
        strncpy(buffer, text, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
    } else if (max_len > 3) {
        strncpy(buffer, text, max_len - 3);
        buffer[max_len - 3] = '\0';
        strcat(buffer, "...");
    } else {
        strncpy(buffer, text, max_len);
        buffer[max_len] = '\0';
    }
    return buffer;
}

int file_present(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/*
 * CSS/APPEARANCE STUBS
 */

char *extract_short_msg(const char *text) {
    static char buffer[128];
    if (!text) return NULL;
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    return buffer;
}

/*
 * TX MENU STUBS
 */

void sort_cfc(void) {
    /* CFC sorting not critical for basic operation */
}

void sort_tx_eq(void) {
    /* TX EQ sorting not critical for basic operation */
}

/*
 * DISPLAY STUBS
 */

void get_screen_size(int *width, int *height) {
    /* Return reasonable defaults */
    if (width) *width = 1920;
    if (height) *height = 1080;
}

/*
 * POSIX STUBS
 */

void sync(void) {
    /* Windows doesn't have sync() - files are automatically synced */
    /* Could call FlushFileBuffers() but not critical */
}

#endif /* _WIN32 */
