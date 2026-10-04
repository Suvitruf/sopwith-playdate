// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_TRACE_H
#define SOPWITH_TRACE_H
// Shared by the port test and the unmodified upstream reference executable.
static int trace_command(int tick)
{
    int command = tick < 20 ? K_ACCEL : 0;
    if (tick == 10 || tick == 11) command |= K_FLAPU;
    if (tick == 22 || tick == 23) command |= K_FLAPD;
    if (tick >= 25 && tick < 60) command |= K_SHOT;
    if (tick == 30 || tick == 45) command |= K_BOMB;
    if (tick == 80) command |= K_HOME;
    return command;
}
#endif
