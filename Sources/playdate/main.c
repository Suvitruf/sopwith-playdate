// SPDX-License-Identifier: GPL-2.0-or-later
#include "pd_api.h"
#include "engine.h"
#include "runtime.h"
#include "services.h"
#include "profile.h"
#include "storage.h"
#include "tone.h"
#include "swmain.h"
#include "swsound.h"
#include "pcsound.h"
#include "video.h"
#include <string.h>
#include <stdatomic.h>
#undef realloc
#undef free

static PlaydateAPI *pd;
static PortClock clock_state;
static PortInput input;
typedef enum { TITLE, FLIGHT, RESULTS, OPTIONS, SETTINGS, CONTROLS, SCORES, CREDITS, LEAVE_FLIGHT } Screen;
static Screen screen, return_screen;
static unsigned selection, help_page, score_mode;
static bool score_daily;
static playmode_t selected_mode = PLAYMODE_COMPUTER;
static PDMenuItem *sound_item;
static bool suspended, restart_requested, options_requested;
static uint32_t report_ms, report_frames, report_ticks, max_update_ms;
static SoundSource *speaker;
static _Atomic uint32_t phase_step, volume_gain;
static PortTone oscillator;
static ProfileStore store;
static SaveIO save_io;
static size_t heap_bytes, heap_peak, heap_allocations;
typedef union { max_align_t align; size_t size; } Allocation;
bool snd_tinnyfilter;

_Static_assert((int)kButtonA == BUTTON_A && (int)kButtonB == BUTTON_B &&
               (int)kButtonLeft == BUTTON_LEFT && (int)kButtonRight == BUTTON_RIGHT &&
               (int)kButtonUp == BUTTON_UP && (int)kButtonDown == BUTTON_DOWN,
               "Update button mapping for this SDK");

void *Port_Realloc(void *ptr, size_t size)
{
    Allocation *old = ptr ? (Allocation *)ptr - 1 : NULL;
    size_t old_size = old ? old->size : 0;
    if (!size) {
        if (old) { pd->system->realloc(old, 0); heap_bytes -= old_size; --heap_allocations; }
        return NULL;
    }
    if (size > SIZE_MAX - sizeof(Allocation)) return NULL;
    Allocation *result = pd->system->realloc(old, sizeof(Allocation) + size);
    if (!result) return NULL;
    result->size = size;
    heap_bytes = heap_bytes - old_size + size;
    if (heap_bytes > heap_peak) heap_peak = heap_bytes;
    if (!old) ++heap_allocations;
    return result + 1;
}
void Port_Free(void *ptr) { Port_Realloc(ptr, 0); }
uint32_t Port_Milliseconds(void) { return pd->system->getCurrentTimeMilliseconds(); }
_Noreturn void Port_Fatal(const char *message)
{
    phase_step = 0;
    pd->system->error("Sopwith: %s", message);
    __builtin_trap();
}
#ifdef TARGET_PLAYDATE
_Noreturn void __assert_func(const char *file, int line, const char *function, const char *expression)
{
    pd->system->logToConsole("Assertion %s:%d %s: %s", file, line, function, expression);
    Port_Fatal("Engine assertion failed; see console log");
}
#endif

static int audio(void *context, int16_t *left, int16_t *right, int count)
{
    (void)context;
    return PortTone_Render(&oscillator, atomic_load_explicit(&phase_step, memory_order_relaxed),
        atomic_load_explicit(&volume_gain, memory_order_relaxed), left, right, count);
}
void Speaker_Init(void)
{
    speaker = pd->sound->addSource(audio, NULL, 1);
    if (!speaker) Port_Fatal("Could not create audio source");
}
void Speaker_Off(void) { phase_step = 0; }
void Speaker_Output(unsigned short divisor)
{
    phase_step = soundflg && !suspended ? PortTone_Step(divisor) : 0;
}
static void apply_settings(void)
{
    soundflg = store.profile.sound;
    volume_gain = store.profile.volume * 800;
    if (sound_item) pd->system->setMenuItemValue(sound_item, soundflg);
    soundoff();
    Speaker_Off();
}
static void save_profile(void)
{
    if (!store.dirty) return;
    if (Profile_Save(&store, &save_io)) {
        pd->system->logToConsole("SOPWITH saved profile generation=%lu", (unsigned long)store.generation);
    } else {
        pd->system->logToConsole("SOPWITH save unavailable status=%d", store.status);
    }
}
static void reset_input_clock(void)
{
    PDButtons held;
    pd->system->getButtonState(&held, NULL, NULL);
    PortInput_Reset(&input, held, pd->system->isCrankDocked() || !store.profile.crank);
    pd->system->getCrankChange();
    uint32_t now = Port_Milliseconds();
    PortClock_Reset(&clock_state, now);
    Port_ResetSoundClock(now);
    report_ms = now;
    report_frames = report_ticks = max_update_ms = 0;
    soundoff();
    Speaker_Off();
}
static void change_screen(Screen next)
{
    screen = next;
    selection = 0;
    reset_input_clock();
}
static void start_game(bool new_campaign)
{
    reset_input_clock();
    Engine_Start(selected_mode, pd->system->getSecondsSinceEpoch(NULL), new_campaign);
    screen = FLIGHT;
    pd->system->logToConsole("SOPWITH start mode=%d level=%d", selected_mode, gamenum);
}
static void open_options(void)
{
    if (screen == TITLE || screen == FLIGHT || screen == RESULTS) return_screen = screen;
    change_screen(OPTIONS);
}
static void menu_restart(void *userdata) { (void)userdata; restart_requested = true; }
static void menu_options(void *userdata) { (void)userdata; options_requested = true; }
static void menu_sound(void *userdata)
{
    (void)userdata;
    store.profile.sound = pd->system->getMenuItemValue(sound_item) != 0;
    store.dirty = true;
    apply_settings();
    save_profile();
}
static void text_at(const char *text, int x, int y)
{
    pd->graphics->drawText(text, strlen(text), kASCIIEncoding, x, y);
}
static void number_at(int number, int x, int y)
{
    char value[12];
    Port_IntText(value, sizeof(value), number);
    text_at(value, x, y);
}
static void row(const char *text, unsigned index, int y)
{
    text_at(selection == index ? ">" : " ", 20, y);
    text_at(text, 43, y);
}
static void save_notice(void)
{
    if (store.status == SAVE_FAILED) text_at("Save failed. Options > Retry save", 18, 213);
    else if (store.status == SAVE_READ_ONLY) text_at("Save unavailable. Changes stay in memory.", 10, 213);
    else if (store.status == SAVE_RECOVERED) text_at("Save recovered. Check your settings.", 20, 213);
}
static void draw_screen(void)
{
    if (screen == FLIGHT) {
        uint8_t *frame = pd->graphics->getFrame();
        Port_ConvertFrame(vid_vram, frame);
        size_t count;
        const PortMarker *markers = Engine_Markers(&count);
        Port_DrawMarkers(frame, markers, count, store.profile.markers);
        pd->graphics->markUpdatedRows(0, 239);
        text_at(selected_mode == PLAYMODE_NOVICE ? "SOPWITH  Practice" : "SOPWITH  Dogfight", 40, 0);
        text_at("Throttle", 245, 0);
        for (int i = 0; i < MAX_THROTTLE; ++i) {
            if (i < consoleplayer->ob_accel) pd->graphics->fillRect(325 + i * 9, 5, 6, 9, kColorBlack);
            else pd->graphics->drawRect(325 + i * 9, 5, 6, 9, kColorBlack);
        }
        // Save failures are shown when leaving flight; no notice covers the HUD.
        return;
    }
    pd->graphics->clear(kColorWhite);
    if (screen == TITLE) {
        text_at("S O P W I T H", 126, 22);
        text_at("Take off. Dogfight. Bomb. Land.", 60, 58);
        text_at("A  Dogfight", 120, 99);
        text_at("B  Practice (novice flight)", 72, 128);
        text_at("Down / Menu: Options", 94, 169);
        text_at("Sopwith 0.4.0", 139, 192);
    } else if (screen == OPTIONS) {
        text_at("OPTIONS", 152, 5);
        const char *items[] = {return_screen == FLIGHT ? "Resume flight" : "Back",
            "Settings", "Mission scores", "Controls", "Credits", "Return to title", "Retry save"};
        unsigned count = store.status == SAVE_FAILED ? 7 : 6;
        for (unsigned i = 0; i < count; ++i) row(items[i], i, 32 + (int)i * 23);
        if (store.status == SAVE_READY) text_at("Up/Down: choose   A: select   B: back", 24, 213);
    } else if (screen == SETTINGS) {
        text_at("SETTINGS", 145, 5);
        const char *labels[] = {"Sound", "Volume", "Pitch", "Crank throttle", "Faction markers", "Back"};
        for (unsigned i = 0; i < 6; ++i) row(labels[i], i, 34 + (int)i * 25);
        text_at(store.profile.sound ? "On" : "Off", 264, 34);
        number_at((int)store.profile.volume * 25, 264, 59); text_at("%", 300, 59);
        text_at(store.profile.inverted ? "Inverted" : "Normal", 264, 84);
        text_at(store.profile.crank ? "On" : "Off", 264, 109);
        text_at(store.profile.markers ? "On" : "Off", 264, 134);
        text_at("Left/Right or A: change   B: save/back", 20, 187);
    } else if (screen == CONTROLS) {
        if (help_page == 0) {
            text_at("FLIGHT CONTROLS  1/2", 94, 5);
            text_at("Up / Down: pitch relative to plane", 20, 35);
            text_at("Right / Left: throttle up / down", 20, 61);
            text_at("A: fire    Release B: drop bomb", 20, 87);
            text_at("Hold B + tap Left: flip", 20, 113);
            text_at("Hold B + tap Right: fly home", 20, 139);
            text_at("Release B before using a chord again.", 20, 165);
        } else {
            text_at("FLIGHT CONTROLS  2/2", 94, 5);
            text_at("Crank: throttle, 30 degrees per step", 20, 35);
            text_at("Settings: invert pitch / disable crank", 20, 61);
            text_at("Markers: V you, square friend, X enemy", 20, 87);
            text_at("Targets and aircraft share markers.", 20, 113);
            text_at("Menu pauses; Options also stops flight.", 20, 139);
            text_at("Release held controls after resuming.", 20, 165);
        }
        text_at("Left/Right: page    A or B: back", 45, 192);
    } else if (screen == SCORES) {
        text_at(score_mode ? "PRACTICE SCORES" : "DOGFIGHT SCORES", 106, 5);
        text_at(score_daily ? "Daily - local, resets 00:00 GMT" : "All-time - local pilot YOU", 47, 32);
        const int *scores = Profile_Scores(&store.profile, score_mode, score_daily,
                                          pd->system->getSecondsSinceEpoch(NULL));
        for (unsigned i = 0; i < PROFILE_SCORES; ++i) {
            char rank[] = "1. YOU";
            rank[0] += (char)i;
            text_at(rank, 85, 58 + (int)i * 22);
            int score = scores[i];
            if (score) number_at(score, 251, 58 + (int)i * 22);
            else text_at("--", 251, 58 + (int)i * 22);
        }
        text_at("Left/Right: mode   Up/Down: period", 30, 175);
        text_at("A or B: back", 135, 194);
    } else if (screen == CREDITS) {
        text_at("SOPWITH - CREDITS", 111, 5);
        text_at("Original: BMB Compuscience Canada", 22, 36);
        text_at("David L. Clark", 22, 62);
        text_at("SDL Sopwith: Simon Howard", 22, 88);
        text_at("Christoph Reichenbach, Jesse Smith", 22, 114);
        text_at("Font: A. Schiffler and Simon Howard", 22, 140);
        text_at("Engine/port GPL v2+; font LGPL v2.1+", 22, 166);
        text_at("Playdate port contributors | A/B: back", 22, 192);
    } else if (screen == LEAVE_FLIGHT) {
        text_at("Leave the current mission?", 71, 67);
        text_at("This unfinished score will not be saved.", 22, 105);
        text_at("A: leave    B: keep playing", 72, 155);
    } else if (screen == RESULTS) {
        bool won = consoleplayer->ob_endsts == WINNER;
        text_at(won ? "MISSION COMPLETE" : "FLIGHT ENDED", 110, 34);
        text_at("Score", 112, 76); number_at(consoleplayer->ob_score.score, 200, 76);
        text_at("Best", 112, 105); number_at(store.profile.scores[selected_mode == PLAYMODE_NOVICE][0], 200, 105);
        text_at(won ? "A: next mission" : "A: try again", 121, 142);
        text_at("B: title    Down: options", 87, 176);
    }
    save_notice();
}
static void navigate(unsigned fresh, unsigned count)
{
    if ((fresh & (BUTTON_UP | BUTTON_DOWN)) == BUTTON_UP) selection = (selection + count - 1) % count;
    if ((fresh & (BUTTON_UP | BUTTON_DOWN)) == BUTTON_DOWN) selection = (selection + 1) % count;
}
static void options_input(unsigned fresh)
{
    unsigned count = store.status == SAVE_FAILED ? 7 : 6;
    navigate(fresh, count);
    if (fresh & BUTTON_B) { save_profile(); change_screen(return_screen); return; }
    if (!(fresh & BUTTON_A)) return;
    switch (selection) {
    case 0: save_profile(); change_screen(return_screen); break;
    case 1: change_screen(SETTINGS); break;
    case 2: score_mode = selected_mode == PLAYMODE_NOVICE; score_daily = false; change_screen(SCORES); break;
    case 3: help_page = 0; change_screen(CONTROLS); break;
    case 4: change_screen(CREDITS); break;
    case 5: change_screen(return_screen == FLIGHT ? LEAVE_FLIGHT : TITLE); break;
    case 6: save_profile(); selection = 0; break;
    }
}
static void settings_input(unsigned fresh)
{
    navigate(fresh, 6);
    if ((fresh & BUTTON_B) || (selection == 5 && (fresh & BUTTON_A))) {
        save_profile(); change_screen(OPTIONS); return;
    }
    unsigned horizontal = fresh & (BUTTON_LEFT | BUTTON_RIGHT);
    if (horizontal == (BUTTON_LEFT | BUTTON_RIGHT)) return;
    if (!(fresh & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_A))) return;
    switch (selection) {
    case 0: store.profile.sound = !store.profile.sound; break;
    case 1:
        if (fresh & BUTTON_LEFT) { if (store.profile.volume) --store.profile.volume; }
        else if (fresh & BUTTON_RIGHT) { if (store.profile.volume < 4) ++store.profile.volume; }
        else store.profile.volume = (store.profile.volume + 1) % 5;
        break;
    case 2: store.profile.inverted = !store.profile.inverted; break;
    case 3: store.profile.crank = !store.profile.crank; break;
    case 4: store.profile.markers = !store.profile.markers; break;
    default: return;
    }
    store.dirty = true;
    apply_settings();
}
static int update(void *userdata)
{
    (void)userdata;
    if (suspended) return 0;
    uint32_t now = Port_Milliseconds();
    PDButtons held, pressed, released;
    pd->system->getButtonState(&held, &pressed, &released);
    bool transitioned = false;
    if (restart_requested) {
        restart_requested = false;
        save_profile(); start_game(true); transitioned = true;
    }
    if (options_requested) {
        options_requested = false;
        save_profile(); open_options(); transitioned = true;
    }
    input.blocked &= held | pressed;
    unsigned fresh = pressed & ~input.blocked;
    if (!transitioned) {
        if (screen == TITLE && (fresh & (BUTTON_A | BUTTON_B))) {
            selected_mode = fresh & BUTTON_A ? PLAYMODE_COMPUTER : PLAYMODE_NOVICE;
            start_game(true);
        } else if ((screen == TITLE || screen == RESULTS) && (fresh & BUTTON_DOWN)) {
            open_options();
        } else if (screen == OPTIONS) options_input(fresh);
        else if (screen == SETTINGS) settings_input(fresh);
        else if (screen == CONTROLS || screen == CREDITS || screen == SCORES) {
            if (fresh & (BUTTON_A | BUTTON_B)) change_screen(OPTIONS);
            else if ((fresh & (BUTTON_LEFT | BUTTON_RIGHT)) == BUTTON_LEFT ||
                     (fresh & (BUTTON_LEFT | BUTTON_RIGHT)) == BUTTON_RIGHT) {
                if (screen == CONTROLS) help_page ^= 1;
                if (screen == SCORES) score_mode ^= 1;
            } else if (screen == SCORES &&
                       ((fresh & (BUTTON_UP | BUTTON_DOWN)) == BUTTON_UP ||
                        (fresh & (BUTTON_UP | BUTTON_DOWN)) == BUTTON_DOWN)) {
                score_daily = !score_daily;
            }
        } else if (screen == LEAVE_FLIGHT) {
            if (fresh & BUTTON_B) change_screen(FLIGHT);
            else if (fresh & BUTTON_A) { save_profile(); change_screen(TITLE); }
        } else if (screen == RESULTS && (fresh & (BUTTON_A | BUTTON_B))) {
            if (fresh & BUTTON_A) start_game(consoleplayer->ob_endsts != WINNER);
            else change_screen(TITLE);
        } else if (screen == FLIGHT) {
            PortInput_Sample(&input, held, pressed, released, pd->system->getCrankChange(),
                pd->system->isCrankDocked() || !store.profile.crank);
            unsigned steps = PortClock_Advance(&clock_state, now);
            for (unsigned i = 0; i < steps && !restart_flag; ++i) {
                int command = PortInput_Take(&input);
                if (store.profile.inverted) {
                    int pitch = command & (K_FLAPU | K_FLAPD);
                    command &= ~(K_FLAPU | K_FLAPD);
                    if (pitch & K_FLAPU) command |= K_FLAPD;
                    if (pitch & K_FLAPD) command |= K_FLAPU;
                }
                Engine_Step(command);
                ++report_ticks;
            }
            Port_UpdateSound(now);
            if (restart_flag) {
                store.dirty |= Profile_RecordScore(&store.profile, selected_mode == PLAYMODE_NOVICE,
                    consoleplayer->ob_score.score, pd->system->getSecondsSinceEpoch(NULL));
                change_screen(RESULTS);
                save_profile();
            }
        }
    }
    draw_screen();
    ++report_frames;
    uint32_t duration = Port_Milliseconds() - now;
    if (duration > max_update_ms) max_update_ms = duration;
    if (Port_Milliseconds() - report_ms >= 5000) {
        pd->system->logToConsole("SOPWITH state=%d frames=%lu ticks=%lu interval_ms=%lu max_update_ms=%lu heap=%lu peak=%lu allocs=%lu",
            screen, (unsigned long)report_frames, (unsigned long)report_ticks,
            (unsigned long)(Port_Milliseconds() - report_ms), (unsigned long)max_update_ms,
            (unsigned long)heap_bytes, (unsigned long)heap_peak, (unsigned long)heap_allocations);
        report_ms = Port_Milliseconds();
        report_frames = report_ticks = max_update_ms = 0;
    }
    return 1;
}
#ifdef _WIN32
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI *api, PDSystemEvent event, uint32_t arg)
{
    (void)arg;
    if (event == kEventInit) {
        pd = api;
        screen = return_screen = TITLE;
        selection = help_page = score_mode = 0;
        score_daily = false;
        suspended = restart_requested = options_requested = false;
        selected_mode = PLAYMODE_COMPUTER;
        heap_bytes = heap_peak = heap_allocations = 0;
        phase_step = 0;
        oscillator = (PortTone){0};
        save_io = Playdate_SaveIO(pd);
        Profile_Load(&store, &save_io);
        sound_item = NULL;
        apply_settings();
        Engine_Init();
        Speaker_Init();
        pd->display->setRefreshRate(30);
        pd->system->setCrankSoundsDisabled(1);
        pd->system->addMenuItem("Restart", menu_restart, NULL);
        sound_item = pd->system->addCheckmarkMenuItem("Sound", store.profile.sound, menu_sound, NULL);
        pd->system->addMenuItem("Options", menu_options, NULL);
        reset_input_clock();
        pd->system->setUpdateCallback(update, NULL);
        pd->system->logToConsole("SOPWITH 0.4.0 build 9 initialized; save_status=%d generation=%lu", store.status, (unsigned long)store.generation);
    } else if (event == kEventPause || event == kEventLock) {
        suspended = true;
        reset_input_clock();
        save_profile();
        pd->system->logToConsole("SOPWITH suspended event=%d", event);
    } else if (event == kEventResume || event == kEventUnlock) {
        suspended = false;
        reset_input_clock();
        pd->system->logToConsole("SOPWITH resumed event=%d", event);
    } else if (event == kEventTerminate) {
        Speaker_Off();
        if (speaker) { pd->sound->removeSource(speaker); speaker = NULL; }
        save_profile();
        Engine_Shutdown();
        pd->system->removeAllMenuItems();
        sound_item = NULL;
        pd->system->logToConsole("SOPWITH shutdown heap=%lu allocs=%lu", (unsigned long)heap_bytes, (unsigned long)heap_allocations);
    }
    return 0;
}
