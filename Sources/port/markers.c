// SPDX-License-Identifier: GPL-2.0-or-later
#include "markers.h"
#include <stdbool.h>

void Port_DrawMarkers(uint8_t *frame, const PortMarker *markers, size_t count, int show_factions)
{
    // 5x5: player chevron, friendly open square, enemy cross. A white border
    // preserves contrast without erasing the aircraft/target below the marker.
    static const uint8_t patterns[][5] = {{0,17,10,4,0}, {31,17,17,17,31}, {17,10,4,10,17}};
    for (size_t i = 0; i < count; ++i) {
        const PortMarker *marker = &markers[i];
        if (marker->kind > MARKER_ENEMY || marker->kind < MARKER_PLAYER ||
            (!show_factions && marker->kind != MARKER_PLAYER)) continue;
        int x = marker->x, y = marker->y;
        if (x < 43 || x > 356 || y < 23 || y > 216) continue;
        for (int dy = -3; dy <= 3; ++dy) {
            for (int dx = -3; dx <= 3; ++dx) {
                uint8_t mask = (uint8_t)(0x80u >> ((x + dx) & 7));
                uint8_t *pixel = frame + (y + dy) * 52 + (x + dx) / 8;
                bool black = dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2 &&
                    (patterns[marker->kind][dy + 2] & (1u << (2 - dx)));
                if (black) *pixel &= (uint8_t)~mask;
                else *pixel |= mask;
            }
        }
    }
}
