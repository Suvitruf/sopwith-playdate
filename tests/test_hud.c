// SPDX-License-Identifier: GPL-2.0-or-later
#include "hud.h"
#include "sw.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static bool black(const uint8_t *frame, int x, int y)
{
    return !(frame[y * 52 + x / 8] & (0x80u >> (x & 7)));
}

void test_hud(void)
{
    uint8_t guarded[16 + 52 * 240 + 16], expected[52 * 240];
    uint8_t *frame = guarded + 16;
    const PortHUD full = {MAXFUEL, MAXROUNDS, MAXBOMBS, MAXCRASH, false};
    const PortHUD empty = {0, 0, 0, 0, false};
    const PortHUD cases[] = {full, empty, {MAXFUEL / 5, 1, 1, 1, false},
        {1, 99, 3, 2, false}, {MAXFUEL, 0, 0, 5, true},
        {INT_MIN, INT_MIN, INT_MIN, INT_MIN, false},
        {INT_MAX, INT_MAX, INT_MAX, INT_MAX, false}};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memset(guarded, 0xa5, sizeof(guarded));
        Port_DrawHUD(frame, &cases[i]);
        for (int n = 0; n < 16; ++n)
            assert(guarded[n] == 0xa5 && guarded[16 + 52 * 240 + n] == 0xa5);
        for (int y = 0; y < 240; ++y) {
            for (int byte = 0; byte < 52; ++byte) {
                if (y < 20 || byte >= 50 || (y < 220 && byte >= 5 && byte < 45))
                    assert(frame[y * 52 + byte] == 0xa5);
            }
        }
    }

    memset(frame, 0xff, 52 * 240);
    Port_DrawHUD(frame, &full);
    assert(black(frame, 15, 104) && black(frame, 15, 178));
    assert(black(frame, 372, 76) && !black(frame, 380, 78)); // five aircraft
    memcpy(expected, frame, sizeof(expected));
    PortHUD over = {INT_MAX, INT_MAX, INT_MAX, INT_MAX, false};
    Port_DrawHUD(frame, &over);
    assert(memcmp(frame, expected, sizeof(expected)) == 0);

    Port_DrawHUD(frame, &empty);
    assert(!black(frame, 15, 104) && !black(frame, 15, 178));
    assert(!black(frame, 372, 76) && black(frame, 378, 76)); // zero aircraft
    memcpy(expected, frame, sizeof(expected));
    PortHUD under = {INT_MIN, INT_MIN, INT_MIN, INT_MIN, false};
    Port_DrawHUD(frame, &under);
    assert(memcmp(frame, expected, sizeof(expected)) == 0);

    // Empty, low and full fuel redraw cleanly, including the non-flashing warning.
    PortHUD low = {MAXFUEL / 5, MAXROUNDS, MAXBOMBS, MAXCRASH, false};
    Port_DrawHUD(frame, &low);
    low.fuel = 1;
    Port_DrawHUD(frame, &low);
    assert(black(frame, 15, 178)); // nonzero fuel retains a visible gauge segment
    Port_DrawHUD(frame, &full);
    memset(expected, 0xff, sizeof(expected));
    Port_DrawHUD(expected, &full);
    assert(memcmp(frame, expected, sizeof(expected)) == 0);

    PortHUD practice = {MAXFUEL, 0, 0, MAXCRASH, true};
    Port_DrawHUD(frame, &practice);
    memcpy(expected, frame, sizeof(expected));
    practice.rounds = MAXROUNDS; practice.bombs = MAXBOMBS;
    Port_DrawHUD(frame, &practice);
    assert(memcmp(frame, expected, sizeof(expected)) == 0);
    practice.unlimited_weapons = false;
    Port_DrawHUD(frame, &practice);
    assert(memcmp(frame + 222 * 52, expected + 222 * 52, 16 * 52) != 0);
}
