// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the actual application and SDK file adapter against a small fake SDK.
#include "pd_api.h"
#include "profile.h"
#include "swmain.h"
#include "swend.h"
#include "pcsound.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef realloc
#undef free

static uint32_t now;
static uint32_t epoch_seconds = 12345;
static PDButtons held, pushed, released;
static float crank;
static bool docked = true;
static int (*update_callback)(void *);
static AudioSourceFunction *audio_callback;
static size_t allocations;
static char drawn[4096];
static uint8_t frame[52 * 240];
static bool menu_silent;
static unsigned flushes, closes;
static int failure;
static const char *names[] = {"profile-a.dat", "profile-b.dat", "profile.tmp"};
typedef struct { uint8_t data[128]; size_t size, pos; bool exists, opened; } File;
static File files[3];
typedef struct { const char *name; int value; PDMenuItemCallbackFunction *callback; void *context; } Menu;
static Menu menu[3]; static unsigned num_menu;
static unsigned index_of(const char *name)
{
    for (unsigned i = 0; i < 3; ++i) if (!strcmp(name, names[i])) return i;
    abort();
}
static int listfiles(const char *path, void (*callback)(const char *, void *), void *context, int hidden)
{
    (void)hidden; assert(!strcmp(path, "/"));
    if (failure == 7) return -1;
    for (unsigned i = 0; i < 3; ++i) if (files[i].exists) callback(names[i], context);
    return 0;
}
static SDFile *open_file(const char *name, FileOptions mode)
{
    File *file = &files[index_of(name)];
    assert(!file->opened);
    if (failure == 1) return NULL;
    if (mode == kFileReadData && !file->exists) return NULL;
    assert(mode == kFileWrite || mode == kFileReadData);
    if (mode == kFileWrite) { file->size = 0; file->exists = true; }
    file->pos = 0; file->opened = true;
    return (SDFile *)file;
}
static int close_file(SDFile *handle)
{
    File *file = (File *)handle; assert(file->opened); file->opened = false; ++closes;
    return failure == 4 ? -1 : 0;
}
static int read_file(SDFile *handle, void *data, unsigned size)
{
    File *file = (File *)handle; assert(file->opened);
    if (failure == 6) return -1;
    if (size > file->size - file->pos) size = (unsigned)(file->size - file->pos);
    memcpy(data, file->data + file->pos, size); file->pos += size;
    return (int)size;
}
static int write_file(SDFile *handle, const void *data, unsigned size)
{
    File *file = (File *)handle; assert(file->opened && size <= sizeof(file->data));
    if (failure == 2) size /= 2;
    memcpy(file->data, data, size); file->size = size;
    return (int)size;
}
static int flush_file(SDFile *file) { (void)file; ++flushes; return failure == 3 ? -1 : 0; }
static const char *file_error(void) { return "injected file failure"; }
static int unlink_file(const char *name, int recursive)
{
    assert(!recursive && strcmp(name, "profile.tmp"));
    File *file = &files[index_of(name)];
    assert(!file->opened && file->exists);
    if (failure == 8) return -1;
    file->exists = false;
    return 0;
}
static int rename_file(const char *from, const char *to)
{
    // Real hardware rejects replacement; POSIX-style fake masked build 6's bug.
    if (failure == 5 || files[index_of(to)].exists) return -1;
    files[index_of(to)] = files[index_of(from)]; files[index_of(from)].exists = false;
    return 0;
}
static void *allocate(void *ptr, size_t size)
{
    if (!size) { if (ptr) --allocations; free(ptr); return NULL; }
    if (!ptr) ++allocations;
    void *result = realloc(ptr, size); assert(result); return result;
}
static void log_message(const char *format, ...) { (void)format; }
static void error_message(const char *format, ...)
{
    va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args); abort();
}
static unsigned int milliseconds(void) { return now; }
static unsigned int seconds(unsigned int *ms) { if (ms) *ms = 0; return epoch_seconds; }
static void set_update(int (*callback)(void *), void *context) { assert(!context); update_callback = callback; }
static void buttons(PDButtons *current, PDButtons *press, PDButtons *release)
{
    if (current) *current = held;
    if (press) *press = pushed;
    if (release) *release = released;
}
static float crank_change(void) { float result = crank; crank = 0; return result; }
static int crank_docked(void) { return docked; }
static int crank_sounds(int disabled) { assert(disabled); return 0; }
static PDMenuItem *add_menu(const char *name, PDMenuItemCallbackFunction *cb, void *ctx)
{
    assert(num_menu < 3); Menu *item = &menu[num_menu++]; *item = (Menu){name, 0, cb, ctx};
    return (PDMenuItem *)item;
}
static PDMenuItem *add_check(const char *name, int value, PDMenuItemCallbackFunction *cb, void *ctx)
{
    PDMenuItem *item = add_menu(name, cb, ctx); ((Menu *)item)->value = value; return item;
}
static void remove_menus(void) { num_menu = 0; }
static int get_menu(PDMenuItem *item) { return ((Menu *)item)->value; }
static void set_menu(PDMenuItem *item, int value) { ((Menu *)item)->value = value; }
static void refresh(float rate) { assert(rate == 30); }
static void clear(LCDColor color) { memset(frame, color == kColorWhite ? 0xff : 0, sizeof(frame)); }
static void rect(int x, int y, int w, int h, LCDColor color) { (void)x;(void)y;(void)w;(void)h;(void)color; }
static int draw_text(const void *text, size_t length, PDStringEncoding encoding, int x, int y)
{
    (void)encoding; (void)x; (void)y;
    assert(strlen(drawn) + length + 2 < sizeof(drawn));
    strncat(drawn, text, length); strcat(drawn, "\n");
    return (int)length;
}
static uint8_t *get_frame(void) { return frame; }
static void dirty(int first, int last) { assert(first == 0 && last == 239); }
static SoundSource *add_audio(AudioSourceFunction *cb, void *context, int stereo)
{
    assert(!context && stereo); audio_callback = cb; return (SoundSource *)&menu_silent;
}
static int remove_audio(SoundSource *source) { assert(source); audio_callback = NULL; return 1; }
static struct playdate_sys system_api = {.realloc=allocate, .logToConsole=log_message, .error=error_message,
    .getCurrentTimeMilliseconds=milliseconds, .getSecondsSinceEpoch=seconds, .setUpdateCallback=set_update,
    .getButtonState=buttons, .getCrankChange=crank_change, .isCrankDocked=crank_docked,
    .setCrankSoundsDisabled=crank_sounds, .addMenuItem=add_menu, .addCheckmarkMenuItem=add_check,
    .getMenuItemValue=get_menu, .setMenuItemValue=set_menu, .removeAllMenuItems=remove_menus};
static struct playdate_file file_api = {.geterr=file_error, .unlink=unlink_file,
    .listfiles=listfiles, .open=open_file, .close=close_file,
    .read=read_file, .write=write_file, .flush=flush_file, .rename=rename_file};
static struct playdate_display display_api = {.setRefreshRate=refresh};
static struct playdate_graphics graphics_api = {.clear=clear, .drawRect=rect, .fillRect=rect,
    .drawText=draw_text, .getFrame=get_frame, .markUpdatedRows=dirty};
static struct playdate_sound sound_api = {.addSource=add_audio, .removeSource=remove_audio};
static PlaydateAPI api = {.system=&system_api, .file=&file_api, .graphics=&graphics_api,
    .display=&display_api, .sound=&sound_api};
static void frame_at(PDButtons buttons_down, unsigned elapsed)
{
    now += elapsed; pushed = buttons_down & ~held; released = held & ~buttons_down; held = buttons_down;
    drawn[0] = 0; update_callback(NULL);
}
static void tap(PDButtons key) { frame_at(key, 33); frame_at(0, 33); }
static bool showing(const char *text) { return strstr(drawn, text) != NULL; }
static void boot(void)
{
    held = pushed = released = 0; crank = 0; docked = true; now = 0;
    eventHandler(&api, kEventInit, 0); frame_at(0, 33);
    assert(showing("S O P W I T H") && allocations > 0 && num_menu == 3);
}
static void shutdown(void)
{
    eventHandler(&api, kEventTerminate, 0);
    assert(allocations == 0 && !audio_callback && !num_menu);
    for (unsigned i = 0; i < 3; ++i) assert(!files[i].opened);
}
static void options(void)
{
    eventHandler(&api, kEventPause, 0);
    assert(!strcmp(menu[2].name, "Options"));
    menu[2].callback(menu[2].context);
    eventHandler(&api, kEventResume, 0); frame_at(0, 33);
    assert(showing("OPTIONS"));
}
static void settings(void) { options(); tap(kButtonDown); tap(kButtonA); assert(showing("SETTINGS")); }
static Profile load_profile(void)
{
    extern SaveIO Playdate_SaveIO(PlaydateAPI *pd);
    SaveIO io = Playdate_SaveIO(&api); ProfileStore store; Profile_Load(&store, &io);
    assert(store.status == SAVE_READY || store.status == SAVE_RECOVERED); return store.profile;
}
static void test_ui_and_persistence(void)
{
    boot(); settings();
    tap(kButtonDown); tap(kButtonRight); // volume 75%
    tap(kButtonDown); tap(kButtonA); // inverted pitch
    tap(kButtonDown); tap(kButtonA); // crank off
    tap(kButtonB); assert(showing("OPTIONS"));
    assert(flushes && closes);
    Profile saved = load_profile();
    assert(saved.volume == 3 && saved.inverted && !saved.crank && saved.sound);
    tap(kButtonB); assert(showing("S O P W I T H"));
    shutdown(); boot(); settings(); assert(showing("Inverted"));
    tap(kButtonB); tap(kButtonB); tap(kButtonA); assert(consoleplayer && playmode == PLAYMODE_COMPUTER);
    // Disabled crank must not accelerate the aircraft.
    docked = false; crank = 90; frame_at(0, 100);
    crank = 90; frame_at(0, 100); assert(consoleplayer->ob_accel == 0);
    frame_at(kButtonRight, 400); assert(consoleplayer->ob_accel == 3); frame_at(0, 100);
    int ticks = countmove;
    frame_at(kButtonB, 33);
    eventHandler(&api, kEventPause, 0); now += 30000;
    eventHandler(&api, kEventResume, 0); frame_at(0, 33);
    assert(countmove == ticks); // no catch-up or stale bomb on resume
    // Pause gate silences the actual source, including its volume ramp.
    Speaker_Output(1193); int16_t samples[512];
    assert(audio_callback(NULL, samples, NULL, 512));
    eventHandler(&api, kEventLock, 0);
    audio_callback(NULL, samples, NULL, 512);
    assert(!audio_callback(NULL, samples, NULL, 512));
    eventHandler(&api, kEventUnlock, 0);
    options(); ticks = countmove;
    frame_at(0, 30000); assert(countmove == ticks);
    tap(kButtonDown); tap(kButtonDown); tap(kButtonDown); tap(kButtonA);
    assert(showing("Release B before using a chord again."));
    tap(kButtonRight); assert(showing("square friend"));
    tap(kButtonB); // Options resets selection
    for (int i = 0; i < 4; ++i) tap(kButtonDown);
    tap(kButtonA); assert(showing("David L. Clark") && showing("A. Schiffler"));
    tap(kButtonB); tap(kButtonB); // resume flight
    consoleplayer->ob_score.score = 1000; winner(consoleplayer); endcount = 1;
    frame_at(0, 100); assert(showing("MISSION COMPLETE"));
    saved = load_profile(); assert(saved.scores[0][0] > 1000 && saved.scores[1][0] == 0);
    int best = saved.scores[0][0];
    assert(Profile_Scores(&saved, 0, true, epoch_seconds)[0] == best); frame_at(0, 1000);
    assert(load_profile().scores[0][1] == 0); // result isn't submitted every frame
    shutdown(); boot();
    assert(load_profile().scores[0][0] == best);
    options(); tap(kButtonDown); tap(kButtonDown); tap(kButtonA);
    assert(showing("DOGFIGHT SCORES") && showing("All-time"));
    tap(kButtonUp); assert(showing("Daily -"));
    epoch_seconds = 86400; frame_at(0, 33);
    assert(showing("Daily -") && showing("--"));
    saved = load_profile();
    assert(Profile_Scores(&saved, 0, true, epoch_seconds)[0] == 0);
    assert(saved.scores[0][0] == best);
    tap(kButtonDown); assert(showing("All-time"));
    tap(kButtonRight); assert(showing("PRACTICE SCORES"));
    shutdown();
}
static void test_sdk_write_failures(void)
{
    extern SaveIO Playdate_SaveIO(PlaydateAPI *pd);
    SaveIO io = Playdate_SaveIO(&api); ProfileStore store;
    Profile_Load(&store, &io);
    File backup[3]; memcpy(backup, files, sizeof(files));
    const int failures[] = {1,2,3,4,5,6,8};
    for (unsigned i = 0; i < sizeof(failures)/sizeof(failures[0]); ++i) {
        memcpy(files, backup, sizeof(files)); failure = 0; Profile_Load(&store, &io);
        store.profile.volume = 1; store.dirty = true;
        int active = store.active_slot; failure = failures[i];
        assert(!Profile_Save(&store, &io));
        for (unsigned i = 0; i < 3; ++i) assert(!files[i].opened);
        assert(memcmp(files[active].data, backup[active].data, PROFILE_BYTES) == 0);
        failure = 0; ProfileStore loaded; Profile_Load(&loaded, &io);
        assert(loaded.profile.volume == 3);
    }
    memcpy(files, backup, sizeof(files)); failure = 7; boot();
    assert(showing("Save unavailable")); shutdown(); failure = 0;
    // Rotate repeatedly through both existing slots, preserving the current one.
    for (unsigned i = 0; i < 10; ++i) {
        Profile_Load(&store, &io);
        int active = store.active_slot;
        File previous = files[active];
        store.profile.volume = i % 5; store.dirty = true;
        assert(Profile_Save(&store, &io));
        assert(memcmp(&files[active], &previous, sizeof(previous)) == 0);
        ProfileStore loaded; Profile_Load(&loaded, &io);
        assert(loaded.profile.volume == i % 5 && loaded.generation == store.generation);
    }
}
int main(void)
{
    test_ui_and_persistence(); test_sdk_write_failures();
    puts("PASS: app screens, settings/scores across relaunch, SDK file failures, pause/lock/audio and cleanup");
}
