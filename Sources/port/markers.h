// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_MARKERS_H
#define SOPWITH_MARKERS_H
#include <stddef.h>
#include <stdint.h>
typedef enum { MARKER_PLAYER, MARKER_FRIEND, MARKER_ENEMY } MarkerKind;
typedef struct { int x, y; MarkerKind kind; } PortMarker;
void Port_DrawMarkers(uint8_t *frame, const PortMarker *markers, size_t count, int show_factions);
#endif
