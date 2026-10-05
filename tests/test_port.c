// SPDX-License-Identifier: GPL-2.0-or-later
#include "engine.h"
#include "runtime.h"
#include "services.h"
#include "swmain.h"
#include "swinit.h"
#include "swcollsn.h"
#include "swend.h"
#include "video.h"
#include "pcsound.h"
#include "trace.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef realloc
#undef free

static uint32_t milliseconds;
static size_t live_bytes, live_allocations, peak_bytes;
typedef union { max_align_t align; size_t size; } Allocation;
void *Port_Realloc(void *ptr, size_t size)
{
    Allocation *old = ptr ? (Allocation *)ptr - 1 : NULL;
    if (old) { live_bytes -= old->size; --live_allocations; }
    if (!size) { free(old); return NULL; }
    Allocation *result = realloc(old, sizeof(*result) + size);
    assert(result);
    result->size = size;
    live_bytes += size;
    if (live_bytes > peak_bytes) peak_bytes = live_bytes;
    ++live_allocations;
    return result + 1;
}
void Port_Free(void *ptr) { Port_Realloc(ptr, 0); }
uint32_t Port_Milliseconds(void) { return milliseconds; }
_Noreturn void Port_Fatal(const char *message) { fprintf(stderr, "%s\n", message); abort(); }
void Speaker_Init(void) {}
void Speaker_Off(void) {}
void Speaker_Output(unsigned short count) { (void)count; }
bool snd_tinnyfilter;
void test_hud(void);

static void test_clock(void)
{
    PortClock clock;
    PortClock_Reset(&clock, 1000);
    assert(PortClock_Advance(&clock, 1099) == 0);
    assert(PortClock_Advance(&clock, 1100) == 1);
    assert(PortClock_Advance(&clock, 1200) == 1);
    assert(PortClock_Advance(&clock, 9000) == 3);
    assert(PortClock_Advance(&clock, 9001) == 0);
    PortClock_Reset(&clock, UINT32_MAX - 49);
    assert(PortClock_Advance(&clock, 50) == 1);
    PortClock_Reset(&clock, 100000);
    assert(PortClock_Advance(&clock, 100033) == 0);
}

static void test_input(void)
{
    PortInput input;
    PortInput_Reset(&input, 0, true);
    PortInput_Sample(&input, BUTTON_A, BUTTON_A, 0, 0, true);
    PortInput_Sample(&input, 0, 0, BUTTON_A, 0, true);
    assert(PortInput_Take(&input) == K_SHOT); // tap entirely between world ticks
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, BUTTON_A, BUTTON_A, 0, 0, true);
    assert(PortInput_Take(&input) == K_SHOT);
    assert(PortInput_Take(&input) == K_SHOT);
    PortInput_Reset(&input, 0, true);
    PortInput_Sample(&input, BUTTON_B, BUTTON_B, 0, 0, true);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, 0, 0, BUTTON_B, 0, true);
    assert(PortInput_Take(&input) == K_BOMB);
    assert(PortInput_Take(&input) == 0);
    // B+Left flips exactly once, consumes the bomb, and suppresses throttle.
    PortInput_Sample(&input, BUTTON_B, BUTTON_B, 0, 0, true);
    PortInput_Sample(&input, BUTTON_B | BUTTON_LEFT, BUTTON_LEFT, 0, 0, true);
    assert(PortInput_Take(&input) == K_FLIP);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, 0, 0, BUTTON_B | BUTTON_LEFT, 0, true);
    assert(PortInput_Take(&input) == 0);
    // A fresh B gesture must allow flipping back, without a bomb on release.
    PortInput_Sample(&input, BUTTON_B, BUTTON_B, 0, 0, true);
    PortInput_Sample(&input, BUTTON_B | BUTTON_LEFT, BUTTON_LEFT, 0, 0, true);
    assert(PortInput_Take(&input) == K_FLIP);
    PortInput_Sample(&input, 0, 0, BUTTON_B | BUTTON_LEFT, 0, true);
    assert(PortInput_Take(&input) == 0);
    // Already-held direction must not trigger a chord or an accidental bomb.
    PortInput_Sample(&input, BUTTON_RIGHT, BUTTON_RIGHT, 0, 0, true);
    PortInput_Sample(&input, BUTTON_RIGHT | BUTTON_B, BUTTON_B, 0, 0, true);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, 0, 0, BUTTON_RIGHT | BUTTON_B, 0, true);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, BUTTON_B | BUTTON_RIGHT, BUTTON_B | BUTTON_RIGHT, 0, 0, true);
    assert(PortInput_Take(&input) == K_HOME);
    PortInput_Sample(&input, 0, 0, BUTTON_B | BUTTON_RIGHT, 0, true);
    assert(PortInput_Take(&input) == 0);
    // Pause cancels gestures and blocks held controls until a fresh press.
    PortInput_Sample(&input, BUTTON_B, BUTTON_B, 0, 0, true);
    PortInput_Reset(&input, BUTTON_B | BUTTON_A, true);
    PortInput_Sample(&input, BUTTON_B | BUTTON_A, 0, 0, 0, true);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, 0, 0, BUTTON_B | BUTTON_A, 0, true);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT,
        BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT, 0, 0, true);
    assert(PortInput_Take(&input) == 0);
    // Undocking movement is discarded, then signed partial rotations accumulate.
    PortInput_Reset(&input, 0, true);
    PortInput_Sample(&input, 0, 0, 0, 180, false);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, 0, 0, 0, 15, false);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, 0, 0, 0, 15, false);
    assert(PortInput_Take(&input) == K_ACCEL);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, 0, 0, 0, -60, false);
    assert(PortInput_Take(&input) == K_DEACC);
    assert(PortInput_Take(&input) == K_DEACC);
    assert(PortInput_Take(&input) == 0);
    PortInput_Sample(&input, BUTTON_LEFT, BUTTON_LEFT, 0, 120, false);
    assert(PortInput_Take(&input) == K_DEACC);
    PortInput_Sample(&input, 0, 0, BUTTON_LEFT, 0, true);
    assert(PortInput_Take(&input) == 0);
}

static void test_pixels(void)
{
    uint8_t mono[52 * 240 + 2], original[320 * 200];
    Vid_ClearBuf();
    vid_vram[0] = 1;
    vid_vram[7] = 2;
    vid_vram[8] = 3;
    vid_vram[199 * 320 + 319] = 2;
    memcpy(original, vid_vram, sizeof(original));
    memset(mono, 0x55, sizeof(mono));
    Port_ConvertFrame(vid_vram, mono + 1);
    assert(mono[0] == 0x55 && mono[sizeof(mono) - 1] == 0x55);
    assert(mono[1 + 20 * 52 + 5] == 0x7e);
    assert(mono[1 + 20 * 52 + 6] == 0x7f);
    assert(mono[1 + 219 * 52 + 44] == 0xfe);
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 52; ++x) {
            if (y < 20 || y >= 220 || x < 5 || x >= 45)
                assert(mono[1 + y * 52 + x] == 0xff);
        }
    }
    assert(memcmp(original, vid_vram, sizeof(original)) == 0);
    // Clipped sprites above/below the viewport, singleton shots, and boxes.
    uint8_t pixels[] = {1,2,3,1};
    sopsym_t symbol = {.data = pixels, .w = 2, .h = 2};
    for (int y = -3; y < 203; ++y) {
        Vid_DispSymbol(-1, y, &symbol, FACTION_PLAYER1);
        Vid_DispSymbol(319, y, &symbol, FACTION_PLAYER1);
    }
    symbol.w = symbol.h = 1;
    Vid_DispSymbol(-1, -1, &symbol, FACTION_PLAYER1);
    Vid_DispSymbol(320, 200, &symbol, FACTION_PLAYER1);
    Vid_Box(-10, 210, 340, 230, 3);
    // Collision uses original sprite occupancy, never presentation dithering.
    symbol.w = symbol.h = 2;
    OBJECTS a = {.ob_symbol=&symbol, .ob_x=50, .ob_y=50};
    OBJECTS b = {.ob_symbol=&symbol, .ob_x=51, .ob_y=50};
    assert(CollisionTest(&a, &b));
    Port_ConvertFrame(vid_vram, mono + 1);
    assert(CollisionTest(&a, &b));
    assert(memcmp(pixels, (uint8_t[]){1,2,3,1}, sizeof(pixels)) == 0);
    b.ob_x = 52;
    assert(!CollisionTest(&a, &b));
}

#include "world_trace.h"

static uint32_t trace(unsigned interval, bool print)
{
    Engine_Start(PLAYMODE_COMPUTER, 12345, true);
    PortClock clock;
    milliseconds = 0;
    PortClock_Reset(&clock, milliseconds);
    bool airborne = false, shot = false, bomb = false, returned = false;
    while (countmove < 300 && !restart_flag) {
        milliseconds += interval;
        unsigned steps = PortClock_Advance(&clock, milliseconds);
        while (steps-- && countmove < 300 && !restart_flag) {
            Engine_Step(trace_command(countmove));
            airborne |= !consoleplayer->ob_athome;
            returned |= airborne && consoleplayer->ob_athome;
            for (OBJECTS *ob = objtop; ob; ob = ob->ob_next) {
                shot |= ob->ob_type == SHOT;
                bomb |= ob->ob_type == BOMB;
            }
            if (print) printf("%d %d %d %d %d %d %d %d %u\n", countmove,
                consoleplayer->ob_x, consoleplayer->ob_y, consoleplayer->ob_speed,
                consoleplayer->ob_state, consoleplayer->ob_rounds, consoleplayer->ob_bombs,
                consoleplayer->ob_score.score, hash_world());
        }
        Port_UpdateSound(milliseconds);
    }
    if (!print) {
        assert(airborne && shot && bomb);
        printf("trace: ticks=%d returned=%d crashes=%d score=%d hash=%08x\n",
            countmove, returned, consoleplayer->ob_crashcnt, consoleplayer->ob_score.score, hash_world());
    }
    return hash_world();
}

static void test_lifetime_and_results(void)
{
    Engine_Start(PLAYMODE_COMPUTER, 12345, true);
    size_t bytes = live_bytes, allocations = live_allocations;
    for (int i = 0; i < 30; ++i) {
        for (int tick = 0; tick < 60; ++tick) Engine_Step(trace_command(tick));
        Engine_Start(PLAYMODE_COMPUTER, 12345, true);
        assert(live_bytes == bytes && live_allocations == allocations);
    }
    winner(consoleplayer);
    endcount = 1;
    Engine_Step(0);
    assert(restart_flag && gamenum == 1);
    int score = consoleplayer->ob_score.score;
    assert(score > 0);
    Engine_Start(PLAYMODE_COMPUTER, 12345, false);
    assert(!restart_flag && gamenum == 1 && consoleplayer->ob_score.score == score);
    loser(consoleplayer);
    endcount = 1;
    Engine_Step(0);
    assert(restart_flag && gamenum == 0);
    Engine_Start(PLAYMODE_NOVICE, 12345, true);
    assert(!restart_flag && consoleplayer->ob_score.score == 0);
}

static void test_landing(void)
{
    Engine_Start(PLAYMODE_NOVICE, 12345, true);
    bool departed = false, landed = false;
    for (int tick = 0; tick < 500 && !restart_flag; ++tick) {
        int command = tick < 4 ? K_ACCEL : 0;
        if (tick == 4 || tick == 5) command |= K_FLAPU;
        if (tick == 15 || tick == 16) command |= K_FLAPD;
        if (tick >= 17 && tick < 22) command |= K_SHOT;
        if (tick == 25) {
            command |= K_HOME;
            // Novice mode has unlimited ammunition. Seed a deficit to exercise
            // the shared landing refill path independently of that rule.
            consoleplayer->ob_rounds = MAXROUNDS - 20;
            consoleplayer->ob_life = MAXFUEL - 200;
        }
        Engine_Step(command);
        departed |= !consoleplayer->ob_athome;
        landed |= departed && consoleplayer->ob_athome;
    }
    printf("landing: departed=%d landed=%d crashes=%d ammo=%d fuel=%d\n",
        departed, landed, consoleplayer->ob_crashcnt, consoleplayer->ob_rounds, consoleplayer->ob_life);
    assert(departed && landed && consoleplayer->ob_crashcnt == 0);
    assert(consoleplayer->ob_rounds == MAXROUNDS && consoleplayer->ob_life == MAXFUEL);
}

static void stress(void)
{
    unsigned restarts = 0, max_objects = 0;
    for (unsigned mission = 0; mission < 12; ++mission) {
        Port_ResetProgress();
        gamenum = (int)(mission % 6);
        Engine_Start(PLAYMODE_COMPUTER, 12345 + mission, false);
        for (unsigned tick = 0; tick < 1000; ++tick) {
            unsigned phase = (unsigned)countmove % 160;
            int command = K_ACCEL | K_SHOT;
            if (phase == 4 || phase == 5) command |= K_FLAPU;
            if (phase == 15 || phase == 16) command |= K_FLAPD;
            if (phase % 17 == 0) command |= K_BOMB;
            if (phase == 60 || phase == 100) command |= K_FLIP;
            if (phase > 110) command = phase == 111 ? K_HOME : 0;
            Engine_Step(command);
            milliseconds += 100;
            Port_UpdateSound(milliseconds);
            unsigned objects = 0;
            for (OBJECTS *ob = objtop; ob; ob = ob->ob_next) {
                assert(++objects < 10000);
                if (ob->ob_next) assert(ob->ob_next->ob_prev == ob);
            }
            if (objects > max_objects) max_objects = objects;
            if (restart_flag) { Engine_Start(PLAYMODE_COMPUTER, 54321 + tick, true); ++restarts; }
        }
    }
    Engine_Shutdown();
    assert(live_bytes == 0 && live_allocations == 0);
    assert(peak_bytes < 4 * 1024 * 1024);
    printf("PASS: 12000 stress ticks, %u automatic restarts, max %u live objects, peak %zu tracked host bytes\n",
           restarts, max_objects, peak_bytes);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    Engine_Init();
    if (argc == 2 && strcmp(argv[1], "--stress") == 0) { stress(); return 0; }
    if (argc == 2 && strcmp(argv[1], "--trace") == 0) {
        trace(100, true);
        Engine_Shutdown();
        assert(live_bytes == 0 && live_allocations == 0);
        return 0;
    }
    test_clock();
    test_input();
    test_pixels();
    test_hud();
    assert(trace(33, false) == trace(17, false));
    assert(trace(100, false) == trace(250, false));
    test_lifetime_and_results();
    test_landing();
    Engine_Shutdown();
    assert(live_bytes == 0 && live_allocations == 0);
    puts("PASS: timing, input, pixels, margin HUD, clipping, collision, deterministic flight, restart lifetime and results");
    return 0;
}
