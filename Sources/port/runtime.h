// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_RUNTIME_H
#define SOPWITH_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>

enum {
    BUTTON_LEFT = 1, BUTTON_RIGHT = 2, BUTTON_UP = 4,
    BUTTON_DOWN = 8, BUTTON_B = 16, BUTTON_A = 32
};
typedef struct {
    uint32_t previous_ms, debt_ms;
} PortClock;
void PortClock_Reset(PortClock *clock, uint32_t now);
unsigned PortClock_Advance(PortClock *clock, uint32_t now);

typedef struct {
    unsigned held, blocked;
    int pending, crank_steps;
    float crank_remainder;
    bool bomb_pending, chord_used, docked;
} PortInput;
void PortInput_Reset(PortInput *input, unsigned held, bool docked);
void PortInput_Sample(PortInput *input, unsigned held, unsigned pressed,
                      unsigned released, float crank, bool docked);
int PortInput_Take(PortInput *input);

// Converts 320x200 indexed pixels to the centered 400x240 Playdate display.
// Destination contains 240 rows of 52 bytes, including untouched white padding.
void Port_ConvertFrame(const uint8_t *source, uint8_t *destination);
#endif
