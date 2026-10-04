// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime.h"
#include "services.h"
#include "sw.h"
#include <string.h>

void PortClock_Reset(PortClock *clock, uint32_t now)
{
    clock->previous_ms = now;
    clock->debt_ms = 0;
}

unsigned PortClock_Advance(PortClock *clock, uint32_t now)
{
    uint32_t elapsed = now - clock->previous_ms;
    clock->previous_ms = now;
    // At most three world ticks per callback. Discard excess stalled time.
    if (elapsed > 300) elapsed = 300;
    clock->debt_ms += elapsed;
    unsigned steps = clock->debt_ms / 100;
    clock->debt_ms %= 100;
    return steps;
}

void PortInput_Reset(PortInput *input, unsigned held, bool docked)
{
    *input = (PortInput){.blocked = held, .docked = docked};
}

static int continuous(unsigned buttons)
{
    int command = (buttons & BUTTON_A) ? K_SHOT : 0;
    unsigned vertical = buttons & (BUTTON_UP | BUTTON_DOWN);
    if (vertical == BUTTON_UP) command |= K_FLAPU;
    if (vertical == BUTTON_DOWN) command |= K_FLAPD;
    if (!(buttons & BUTTON_B)) {
        unsigned horizontal = buttons & (BUTTON_LEFT | BUTTON_RIGHT);
        if (horizontal == BUTTON_LEFT) command |= K_DEACC;
        if (horizontal == BUTTON_RIGHT) command |= K_ACCEL;
    }
    return command;
}

void PortInput_Sample(PortInput *input, unsigned held, unsigned pressed,
                      unsigned released, float crank, bool docked)
{
    input->blocked &= ~released;
    input->blocked &= held | pressed;
    held &= ~input->blocked;
    pressed &= ~input->blocked;
    input->held = held;
    const unsigned horizontal = BUTTON_LEFT | BUTTON_RIGHT;
    if (pressed & BUTTON_B) {
        input->bomb_pending = !(held & horizontal);
        input->chord_used = false;
    }
    // Include a B press even when its release occurred within this callback.
    if ((held | pressed) & BUTTON_B) {
        input->pending &= ~(K_ACCEL | K_DEACC);
        unsigned edge = pressed & horizontal;
        if (!input->chord_used && edge && (held & horizontal) != horizontal) {
            if (edge == BUTTON_LEFT) input->pending |= K_FLIP;
            if (edge == BUTTON_RIGHT) input->pending |= K_HOME;
            input->bomb_pending = false;
            input->chord_used = true;
        }
    }
    if (released & BUTTON_B) {
        if (input->bomb_pending) input->pending |= K_BOMB;
        input->bomb_pending = false;
        input->chord_used = false;
    }
    input->pending |= continuous(held | pressed);
    if ((held & horizontal) == horizontal)
        input->pending &= ~(K_ACCEL | K_DEACC);
    if ((held & (BUTTON_UP | BUTTON_DOWN)) == (BUTTON_UP | BUTTON_DOWN))
        input->pending &= ~(K_FLAPU | K_FLAPD);

    if (docked != input->docked || docked || ((held | pressed) & (horizontal | BUTTON_B))) {
        input->crank_remainder = 0;
        input->crank_steps = 0;
    } else {
        // Accumulate small movements; ignore sub-degree sensor jitter.
        if (crank > 0.1f || crank < -0.1f) input->crank_remainder += crank;
        if (input->crank_remainder > 120) input->crank_remainder = 120;
        if (input->crank_remainder < -120) input->crank_remainder = -120;
        int steps = (int)(input->crank_remainder / 30.0f);
        input->crank_remainder -= steps * 30.0f;
        input->crank_steps = clamp_range(-4, input->crank_steps + steps, 4);
    }
    input->docked = docked;
}

int PortInput_Take(PortInput *input)
{
    int command = continuous(input->held) | input->pending;
    input->pending = 0;
    if (!(command & (K_ACCEL | K_DEACC)) && !(input->held & BUTTON_B)) {
        if (input->crank_steps > 0) { command |= K_ACCEL; --input->crank_steps; }
        if (input->crank_steps < 0) { command |= K_DEACC; ++input->crank_steps; }
    }
    if ((command & (K_FLAPU | K_FLAPD)) == (K_FLAPU | K_FLAPD))
        command &= ~(K_FLAPU | K_FLAPD);
    if ((command & (K_ACCEL | K_DEACC)) == (K_ACCEL | K_DEACC))
        command &= ~(K_ACCEL | K_DEACC);
    return command;
}

void Port_ConvertFrame(const uint8_t *source, uint8_t *destination)
{
    memset(destination, 0xff, 52 * 240);
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            unsigned color = source[y * 320 + x] & 3;
            bool black = color != 0;
            // Keep fine one-pixel details; dither only the interiors of index 2.
            if (color == 2 && ((x + y) & 1) && x > 0 && x < 319 && y > 0 && y < 199) {
                black = !(source[y * 320 + x - 1] && source[y * 320 + x + 1]
                       && source[(y - 1) * 320 + x] && source[(y + 1) * 320 + x]);
            }
            if (black) destination[(y + 20) * 52 + (x + 40) / 8] &=
                (uint8_t)~(0x80u >> ((x + 40) & 7));
        }
    }
}

void Port_IntText(char *out, size_t size, int value)
{
    char digits[12];
    unsigned number = value < 0 ? 0u - (unsigned)value : (unsigned)value;
    size_t n = 0, used = 0;
    do { digits[n++] = (char)('0' + number % 10); number /= 10; } while (number);
    if (!size) return;
    if (value < 0 && used + 1 < size) out[used++] = '-';
    while (n && used + 1 < size) out[used++] = digits[--n];
    out[used] = '\0';
}
