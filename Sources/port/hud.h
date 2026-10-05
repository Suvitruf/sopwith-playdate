// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_HUD_H
#define SOPWITH_HUD_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int fuel, rounds, bombs, aircraft;
    bool unlimited_weapons;
} PortHUD;

// Draw only in the side and bottom margins of the 52-byte-stride LCD frame.
// The 320x200 game image at (40,20), top strip and row padding are untouched.
void Port_DrawHUD(uint8_t *frame, const PortHUD *hud);
#endif
