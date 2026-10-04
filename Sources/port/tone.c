// SPDX-License-Identifier: GPL-2.0-or-later
#include "tone.h"

uint32_t PortTone_Step(unsigned divisor)
{
    if (!divisor || divisor > 65535 || 1193280u >= 22050u * divisor) return 0;
    return (uint32_t)(((uint64_t)1193280 << 32) / (44100ull * divisor));
}
int PortTone_Render(PortTone *tone, uint32_t step, unsigned gain, int16_t *left, int16_t *right, int count)
{
    // Limit amplitude and ramp volume changes over at most 400 samples (~9 ms).
    int target = step ? (int)(gain > 3200 ? 3200 : gain) : 0;
    if (step) tone->last_step = step;
    int audible = 0;
    for (int i = 0; i < count; ++i) {
        if (tone->gain < target) { tone->gain += 8; if (tone->gain > target) tone->gain = target; }
        if (tone->gain > target) { tone->gain -= 8; if (tone->gain < target) tone->gain = target; }
        tone->phase += tone->last_step;
        int16_t value = (int16_t)((tone->phase & 0x80000000u) ? tone->gain : -tone->gain);
        left[i] = value;
        if (right) right[i] = value;
        audible |= value != 0;
    }
    return audible;
}
