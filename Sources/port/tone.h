// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_TONE_H
#define SOPWITH_TONE_H
#include <stdint.h>
typedef struct { uint32_t phase, last_step; int gain; } PortTone;
uint32_t PortTone_Step(unsigned divisor);
int PortTone_Render(PortTone *tone, uint32_t step, unsigned gain, int16_t *left, int16_t *right, int count);
#endif
