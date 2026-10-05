// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_PAUSE_PANEL_H
#define SOPWITH_PAUSE_PANEL_H
#include "hud.h"

typedef enum {
    PORT_PAUSE_TITLE, PORT_PAUSE_FLIGHT, PORT_PAUSE_RESULTS, PORT_PAUSE_MENU
} PortPauseView;

typedef struct {
    PortPauseView view;
    PortHUD resources;
    int score;
    bool won;
    const char *menu_title;
} PortPausePanel;

// Draw a 400x240 menu bitmap. Content stays in the left 192 pixels;
// the remaining visible pixels are white and row padding is untouched.
void Port_DrawPausePanel(uint8_t *data, int rowbytes, const PortPausePanel *panel);
#endif
