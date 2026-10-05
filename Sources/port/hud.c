// SPDX-License-Identifier: GPL-2.0-or-later
#include "hud.h"
#include "runtime.h"
#include "sw.h"
// Reuse the original LGPL-2.1-or-later font; notices remain in font.h.
#include "font.h"
#include <string.h>

static int bounded(int value, int maximum)
{
    return value < 0 ? 0 : value > maximum ? maximum : value;
}

static void pixel(uint8_t *frame, int x, int y, bool black)
{
    if (x < 0 || x >= 400 || y < 20 || y >= 240 ||
        (x >= 40 && x < 360 && y < 220)) return;
    uint8_t *byte = &frame[y * 52 + x / 8];
    uint8_t mask = (uint8_t)(0x80u >> (x & 7));
    if (black) *byte &= (uint8_t)~mask;
    else *byte |= mask;
}

static void rect(uint8_t *frame, int x, int y, int width, int height, bool black)
{
    for (int row = y; row < y + height; ++row)
        for (int col = x; col < x + width; ++col) pixel(frame, col, row, black);
}

static void glyph(uint8_t *frame, unsigned ch, int x, int y, int scale)
{
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            if (font_data[ch * 8 + row] & (0x80u >> col))
                rect(frame, x + col * scale, y + row * scale, scale, scale, true);
        }
    }
}

static void text(uint8_t *frame, const char *value, int x, int y, int scale)
{
    for (; *value; ++value, x += 8 * scale)
        glyph(frame, (unsigned char)*value, x, y, scale);
}

static void number(uint8_t *frame, int value, int center, int y, int scale)
{
    char label[12];
    Port_IntText(label, sizeof(label), value);
    text(frame, label, center - (int)strlen(label) * 4 * scale, y, scale);
}

void Port_DrawHUD(uint8_t *frame, const PortHUD *hud)
{
    // Clearing these owned margins prevents stale digits and low-fuel notices.
    for (int y = 20; y < 240; ++y) {
        if (y < 220) {
            memset(frame + y * 52, 0xff, 5);
            memset(frame + y * 52 + 45, 0xff, 5);
        } else memset(frame + y * 52, 0xff, 50);
    }

    int fuel = bounded(hud->fuel, MAXFUEL);
    int aircraft = bounded(hud->aircraft, MAXCRASH);
    text(frame, "FUEL", 4, 58, 1);
    char percent[12];
    // A positive remainder is never labelled empty before the engine runs out.
    Port_IntText(percent, sizeof(percent), (fuel * 100 + MAXFUEL - 1) / MAXFUEL);
    size_t length = strlen(percent);
    percent[length] = '%'; percent[length + 1] = '\0';
    text(frame, percent, 20 - (int)(length + 1) * 4, 78, 1);
    rect(frame, 9, 99, 22, 84, true);
    rect(frame, 11, 101, 18, 80, false);
    int fill = (fuel * 76 + MAXFUEL - 1) / MAXFUEL;
    rect(frame, 13, 179 - fill, 14, fill, true);
    if (fuel <= MAXFUEL / 5) text(frame, fuel ? "LOW" : "OUT", 8, 193, 1);

    text(frame, "LIVES", 360, 58, 1);
    number(frame, aircraft, 380, 76, 2);

    text(frame, "AMMO", 48, 227, 1);
    text(frame, "BOMBS", 236, 227, 1);
    if (hud->unlimited_weapons) {
        // The original font's CP437 infinity glyph, at twice its native size.
        glyph(frame, 236, 116, 222, 2);
        glyph(frame, 236, 300, 222, 2);
    } else {
        number(frame, bounded(hud->rounds, MAXROUNDS), 124, 222, 2);
        number(frame, bounded(hud->bombs, MAXBOMBS), 308, 222, 2);
    }
}
