// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_ENGINE_H
#define SOPWITH_ENGINE_H
#include "sw.h"
#include "markers.h"
const PortMarker *Engine_Markers(size_t *count);
void Engine_Init(void);
void Engine_Shutdown(void);
void Engine_Start(playmode_t mode, unsigned seed, bool new_campaign);
void Engine_Step(int command);
void Port_ResetProgress(void);
void Port_ResetSoundClock(uint32_t now);
void Port_UpdateSound(uint32_t now);
#endif
