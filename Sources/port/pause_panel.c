// SPDX-License-Identifier: GPL-2.0-or-later
#include "pause_panel.h"
#include "runtime.h"
#include "sw.h"
// Reuse the original LGPL-2.1-or-later font; notices remain in font.h.
#include "font.h"
#include <string.h>

static int bounded(int value, int maximum)
{
    return value < 0 ? 0 : value > maximum ? maximum : value;
}

static void text(uint8_t *data, int stride, const char *value, int x, int y, int scale)
{
    for (; *value && x < 192; ++value, x += 8 * scale) {
        const uint8_t *glyph = &font_data[(unsigned char)*value * 8];
        for (int row = 0; row < 8 * scale && y + row < 240; ++row) {
            for (int col = 0; col < 8 * scale && x + col < 192; ++col) {
                if (glyph[row / scale] & (0x80u >> (col / scale)))
                    data[(y + row) * stride + (x + col) / 8] &= (uint8_t)~(0x80u >> ((x + col) & 7));
            }
        }
    }
}

static void number(uint8_t *data, int stride, int value, int x, int y)
{
    char label[12];
    Port_IntText(label, sizeof(label), value);
    text(data, stride, label, x, y, 2);
}

static void weapon_help(uint8_t *data, int stride)
{
    text(data, stride, "A: FIRE", 8, 175, 1);
    text(data, stride, "RELEASE B: BOMB", 8, 187, 1);
    text(data, stride, "HOLD B + LEFT: FLIP", 8, 199, 1);
    text(data, stride, "HOLD B + RIGHT: HOME", 8, 211, 1);
    text(data, stride, "RELEASE B AFTER A CHORD", 8, 228, 1);
}

void Port_DrawPausePanel(uint8_t *data, int rowbytes, const PortPausePanel *panel)
{
    if (!data || rowbytes < 50) return;
    for (int y = 0; y < 240; ++y) memset(data + y * rowbytes, 0xff, 50);
    text(data, rowbytes, "SOPWITH", 8, 10, 2);
    memset(data + 48 * rowbytes + 1, 0, 22);

    if (panel->view == PORT_PAUSE_FLIGHT || panel->view == PORT_PAUSE_RESULTS) {
        text(data, rowbytes, panel->resources.unlimited_weapons ? "PRACTICE" : "DOGFIGHT", 8, 34, 1);
        text(data, rowbytes, "SCORE", 8, 58, 1);
        number(data, rowbytes, panel->score, 8, 72);
    }
    if (panel->view == PORT_PAUSE_FLIGHT) {
        int fuel = bounded(panel->resources.fuel, MAXFUEL);
        char percent[12];
        Port_IntText(percent, sizeof(percent), (fuel * 100 + MAXFUEL - 1) / MAXFUEL);
        size_t length = strlen(percent);
        percent[length] = '%'; percent[length + 1] = '\0';
        text(data, rowbytes, "FUEL", 8, 99, 1);
        text(data, rowbytes, percent, 8, 112, 2);
        text(data, rowbytes, "LIVES", 108, 99, 1);
        number(data, rowbytes, bounded(panel->resources.aircraft, MAXCRASH), 108, 112);
        if (panel->resources.unlimited_weapons) {
            text(data, rowbytes, "AMMO / BOMBS", 8, 135, 1);
            text(data, rowbytes, "UNLIMITED", 8, 148, 2);
        } else {
            text(data, rowbytes, "AMMO", 8, 135, 1);
            number(data, rowbytes, bounded(panel->resources.rounds, MAXROUNDS), 8, 148);
            text(data, rowbytes, "BOMBS", 108, 135, 1);
            number(data, rowbytes, bounded(panel->resources.bombs, MAXBOMBS), 108, 148);
        }
        weapon_help(data, rowbytes);
    } else if (panel->view == PORT_PAUSE_RESULTS) {
        text(data, rowbytes, panel->won ? "MISSION COMPLETE" : "FLIGHT ENDED", 8, 105, 1);
        text(data, rowbytes, panel->won ? "A: NEXT MISSION" : "A: TRY AGAIN", 8, 135, 1);
        text(data, rowbytes, "B: TITLE", 8, 151, 1);
        text(data, rowbytes, "DOWN: OPTIONS", 8, 167, 1);
        text(data, rowbytes, "CLOSE MENU TO CONTINUE", 8, 228, 1);
    } else if (panel->view == PORT_PAUSE_TITLE) {
        text(data, rowbytes, "READY TO FLY", 8, 34, 1);
        text(data, rowbytes, "A: DOGFIGHT", 8, 62, 1);
        text(data, rowbytes, "B: PRACTICE", 8, 80, 1);
        text(data, rowbytes, "  UNLIMITED WEAPONS", 8, 94, 1);
        text(data, rowbytes, "DOWN: OPTIONS", 8, 116, 1);
        text(data, rowbytes, "LEFT / RIGHT: THROTTLE", 8, 141, 1);
        text(data, rowbytes, "UP / DOWN: PITCH", 8, 155, 1);
        weapon_help(data, rowbytes);
    } else {
        text(data, rowbytes, panel->menu_title ? panel->menu_title : "OPTIONS", 8, 34, 1);
        text(data, rowbytes, "CLOSE THIS MENU", 8, 72, 1);
        text(data, rowbytes, "TO CONTINUE", 8, 88, 1);
        text(data, rowbytes, "FLIGHT HELP:", 8, 120, 1);
        text(data, rowbytes, "OPTIONS > CONTROLS", 8, 136, 1);
        text(data, rowbytes, "SETTINGS:", 8, 170, 1);
        text(data, rowbytes, "SOUND / VOLUME", 8, 186, 1);
        text(data, rowbytes, "PITCH / CRANK / MARKERS", 8, 202, 1);
    }
}
