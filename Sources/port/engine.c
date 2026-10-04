// SPDX-License-Identifier: GPL-2.0-or-later
#include "engine.h"
#include "swmain.h"
#include "swinit.h"
#include "swmove.h"
#include "swgrpha.h"
#include "swcollsn.h"
#include "swsound.h"
#include "swsymbol.h"
#include "video.h"
#include "timer.h"
#include "swend.h"
#include "swtitle.h"
#include "swobject.h"

static uint8_t framebuffer[320 * 200];
// Bounded visual tags; excess markers never affect the engine or collisions.
static PortMarker markers[100];
static size_t marker_count;

const PortMarker *Engine_Markers(size_t *count)
{
    *count = marker_count;
    return markers;
}

void Engine_Init(void)
{
    vid_vram = framebuffer;
    vid_pitch = 320;
    GenerateSymbols();
}

void Engine_Shutdown(void)
{
    initsndt();
    OBJECTS *lists[] = {objtop, objfree, deltop};
    for (unsigned i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i) {
        for (OBJECTS *ob = lists[i], *next; ob; ob = next) {
            next = ob->ob_next;
            Port_Free(ob);
        }
    }
    objtop = objbot = objfree = deltop = delbot = consoleplayer = NULL;
    Port_Free(ground);
    ground = NULL;
    for (symset_t *set = all_symsets; set; set = set->next) {
        for (unsigned i = 0; i < 8; ++i) {
            Port_Free(set->sym[i].data);
            set->sym[i].data = NULL;
        }
    }
    all_symsets = NULL;
    marker_count = 0;
}

void Engine_Start(playmode_t mode, unsigned seed, bool new_campaign)
{
    if (mode != PLAYMODE_NOVICE && mode != PLAYMODE_SINGLE && mode != PLAYMODE_COMPUTER)
        Port_Fatal("Unsupported game mode");
    if (new_campaign) Port_ResetProgress();
    playmode = mode;
    player = 0;
    explseed = seed;
    restart_flag = false;
    titleflg = false;
    endstat = endcount = dispcnt = 0;
    keydelay = 1;
    swinitlevel();
    Vid_ClearBuf();
    marker_count = 0;
    Port_ResetSoundClock(Port_Milliseconds());
}

void Engine_Step(int command)
{
    if (restart_flag) return;
    latest_player_commands[player][countmove % MAX_NET_LAG] = command;
    latest_player_time[player] = countmove + 1;
    swmove();
    swdisp();
    swcollsn();
    swsound();
}

// Desktop exit/keyboard services have no reachable menu path in this port.
// Fail explicitly if a future change tries to invoke desktop process exit.
void swend(char *message, bool update)
{
    (void)update;
    Port_Fatal(message ? message : "Unexpected desktop exit path");
}
bool ctlbreak(void) { return false; }
int Timer_GetMS(void) { return (int)(Port_Milliseconds() & 0x7fffffffu); }
void Vid_Update(void)
{
    // Capture at the render boundary, before collision can move/delete objects.
    marker_count = 0;
    for (OBJECTS *ob = objtop; ob && marker_count < 100; ob = ob->ob_next) {
        if (!ob->ob_drwflg || !ob->ob_symbol || ob->ob_faction == FACTION_NONE) continue;
        if (ob->ob_type == PLANE) {
            if (PlaneIsKilled(ob->ob_state) || ob->ob_state >= FINISHED) continue;
        } else if (ob->ob_type == TARGET) {
            if (ob->ob_state != STANDING) continue;
        } else if (ob->ob_type != BALLOON || ob->ob_state != FLYING) continue;
        int x = 40 + ob->ob_x - displx + ob->ob_symbol->w / 2;
        int y = 20 + 199 - ob->ob_y - 5;
        if (x < 43 || x > 356 || y < 23 || y > 216) continue;
        markers[marker_count++] = (PortMarker){x, y, ob == consoleplayer ? MARKER_PLAYER :
            ob->ob_faction == consoleplayer->ob_faction ? MARKER_FRIEND : MARKER_ENEMY};
    }
}
