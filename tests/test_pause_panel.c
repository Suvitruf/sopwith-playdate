// SPDX-License-Identifier: GPL-2.0-or-later
#include "pause_panel.h"
#include "sw.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

void test_pause_panel(void)
{
    enum { STRIDE = 56, BYTES = STRIDE * 240 };
    uint8_t guarded[16 + BYTES + 16], fresh[BYTES];
    uint8_t *data = guarded + 16;
    PortPausePanel panels[] = {
        {.view = PORT_PAUSE_TITLE},
        {.view = PORT_PAUSE_FLIGHT, .resources = {MAXFUEL, MAXROUNDS, MAXBOMBS, MAXCRASH, false}, .score = INT_MIN},
        {.view = PORT_PAUSE_FLIGHT, .resources = {0, 0, 0, 0, true}, .score = INT_MAX},
        {.view = PORT_PAUSE_RESULTS, .score = -500},
        {.view = PORT_PAUSE_RESULTS, .score = 12000, .won = true},
        {.view = PORT_PAUSE_MENU, .menu_title = "SETTINGS"},
        {.view = PORT_PAUSE_MENU, .menu_title = "A LONG TITLE THAT MUST STAY OUT OF THE SYSTEM MENU"},
    };
    for (size_t i = 0; i < sizeof(panels) / sizeof(panels[0]); ++i) {
        memset(guarded, 0xa5, sizeof(guarded));
        uint8_t before[sizeof(panels[i])];
        memcpy(before, &panels[i], sizeof(before));
        Port_DrawPausePanel(data, STRIDE, &panels[i]);
        assert(memcmp(before, &panels[i], sizeof(before)) == 0);
        for (int n = 0; n < 16; ++n)
            assert(guarded[n] == 0xa5 && guarded[16 + BYTES + n] == 0xa5);
        for (int y = 0; y < 240; ++y) {
            for (int byte = 24; byte < 50; ++byte) assert(data[y * STRIDE + byte] == 0xff);
            for (int byte = 50; byte < STRIDE; ++byte) assert(data[y * STRIDE + byte] == 0xa5);
        }
    }

    // Redrawing a simpler screen must remove the previous flight resources.
    memset(data, 0xff, BYTES); memset(fresh, 0xff, sizeof(fresh));
    Port_DrawPausePanel(data, STRIDE, &panels[1]);
    Port_DrawPausePanel(data, STRIDE, &panels[0]);
    Port_DrawPausePanel(fresh, STRIDE, &panels[0]);
    assert(memcmp(data, fresh, BYTES) == 0);

    // Practice labels unlimited weapons regardless of the numeric counters.
    PortPausePanel practice = panels[2];
    Port_DrawPausePanel(data, STRIDE, &practice);
    practice.resources.rounds = MAXROUNDS; practice.resources.bombs = MAXBOMBS;
    Port_DrawPausePanel(fresh, STRIDE, &practice);
    assert(memcmp(data, fresh, BYTES) == 0);
    practice.resources.unlimited_weapons = false;
    Port_DrawPausePanel(fresh, STRIDE, &practice);
    assert(memcmp(data, fresh, BYTES) != 0);

    PortPausePanel low = panels[1];
    low.resources = (PortHUD){0, 0, 0, 0, false};
    Port_DrawPausePanel(data, STRIDE, &low);
    low.resources = (PortHUD){INT_MIN, INT_MIN, INT_MIN, INT_MIN, false};
    Port_DrawPausePanel(fresh, STRIDE, &low);
    assert(memcmp(data, fresh, BYTES) == 0);
    low.resources = (PortHUD){MAXFUEL, MAXROUNDS, MAXBOMBS, MAXCRASH, false};
    Port_DrawPausePanel(data, STRIDE, &low);
    low.resources = (PortHUD){INT_MAX, INT_MAX, INT_MAX, INT_MAX, false};
    Port_DrawPausePanel(fresh, STRIDE, &low);
    assert(memcmp(data, fresh, BYTES) == 0);
}
