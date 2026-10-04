// SPDX-License-Identifier: GPL-2.0-or-later
// Drives the unmodified pinned desktop engine, linked with its real SDL backend.
#include "sw.h"
#include "swmain.h"
#include "swinit.h"
#include "swmove.h"
#include "swgrpha.h"
#include "swcollsn.h"
#include "swsound.h"
#include "video.h"
#include "trace.h"
#include "world_trace.h"
#include <SDL.h>

int main(void)
{
    char *args[] = {"sopwith-reference", "-c", "-q", NULL};
    swinit(3, args);
    explseed = 12345;
    swinitlevel();
    while (countmove < 300 && !restart_flag) {
        latest_player_commands[player][countmove % MAX_NET_LAG] = trace_command(countmove);
        latest_player_time[player] = countmove + 1;
        swmove(); swdisp(); swcollsn(); swsound();
        printf("%d %d %d %d %d %d %d %d %u\n", countmove,
            consoleplayer->ob_x, consoleplayer->ob_y, consoleplayer->ob_speed,
            consoleplayer->ob_state, consoleplayer->ob_rounds, consoleplayer->ob_bombs,
            consoleplayer->ob_score.score, hash_world());
    }
    SDL_Quit();
    return 0;
}
