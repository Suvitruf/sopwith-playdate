// SPDX-License-Identifier: GPL-2.0-or-later
#include "profile.h"
#include "tone.h"
#include "markers.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t data[128]; size_t size; bool exists; } File;
typedef struct { File files[3]; int fail; unsigned writes; } Disk;
static int slot(const char *name)
{
    if (!strcmp(name, "profile-a.dat")) return 0;
    if (!strcmp(name, "profile-b.dat")) return 1;
    assert(!strcmp(name, "profile.tmp")); return 2;
}
static SaveRead read_file(void *context, const char *name, uint8_t *bytes, size_t capacity, size_t *size)
{
    Disk *disk = context;
    int index = slot(name);
    if (disk->fail == 1 || (disk->fail == 4 && index == 2)) return SAVE_IO_ERROR;
    File *file = &disk->files[index];
    if (!file->exists) return SAVE_MISSING;
    *size = file->size < capacity ? file->size : capacity;
    memcpy(bytes, file->data, *size);
    return SAVE_OK;
}
static bool write_file(void *context, const char *name, const uint8_t *bytes, size_t size)
{
    Disk *disk = context;
    File *file = &disk->files[slot(name)];
    ++disk->writes;
    file->exists = true;
    file->size = disk->fail == 2 ? size / 2 : size;
    memcpy(file->data, bytes, file->size);
    if (disk->fail == 3) file->data[20] ^= 1;
    return disk->fail != 2;
}
static bool rename_file(void *context, const char *from, const char *to)
{
    Disk *disk = context;
    if (disk->fail == 5) return false;
    disk->files[slot(to)] = disk->files[slot(from)];
    disk->files[slot(from)].exists = false;
    return true;
}
static SaveIO io_for(Disk *disk)
{
    return (SaveIO){disk, read_file, write_file, rename_file};
}
static void checksum(uint8_t *bytes)
{
    uint32_t crc = UINT32_MAX;
    unsigned payload = bytes[6] - 4;
    for (unsigned i = 0; i < payload; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = crc & 1 ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
    crc = ~crc;
    for (unsigned i = 0; i < 4; ++i) bytes[payload + i] = (uint8_t)(crc >> (8 * i));
}
static void test_codec(void)
{
    Profile profile, decoded;
    Profile_Defaults(&profile);
    assert(profile.sound && profile.crank && profile.markers && !profile.inverted && profile.volume == 2);
    assert(Profile_AddScore(&profile, 0, 120));
    assert(Profile_AddScore(&profile, 0, 300));
    assert(Profile_AddScore(&profile, 1, 900));
    assert(!Profile_AddScore(&profile, 0, -1));
    assert(!Profile_AddScore(&profile, 2, 99));
    assert(profile.scores[0][0] == 300 && profile.scores[0][1] == 120 && profile.scores[1][0] == 900);
    for (int i = 1; i < 8; ++i) Profile_AddScore(&profile, 0, i);
    assert(profile.scores[0][4] == 5);
    uint8_t bytes[PROFILE_BYTES + 1]; uint32_t generation;
    assert(Profile_Encode(&profile, UINT32_MAX, bytes));
    assert(Profile_Decode(&decoded, &generation, bytes, PROFILE_BYTES) == SAVE_OK);
    assert(generation == UINT32_MAX && decoded.scores[0][0] == 300 && decoded.scores[0][4] == 5);
    for (size_t size = 0; size < PROFILE_BYTES; ++size)
        assert(Profile_Decode(&decoded, &generation, bytes, size) == SAVE_INVALID);
    assert(Profile_Decode(&decoded, &generation, bytes, sizeof(bytes)) == SAVE_INVALID);
    bytes[22] ^= 1;
    assert(Profile_Decode(&decoded, &generation, bytes, PROFILE_BYTES) == SAVE_INVALID);
    bytes[4] = 3;
    assert(Profile_Decode(&decoded, &generation, bytes, PROFILE_BYTES) == SAVE_FUTURE);
    assert(Profile_Encode(&profile, 3, bytes));
    bytes[13] = 5; checksum(bytes);
    assert(Profile_Decode(&decoded, &generation, bytes, PROFILE_BYTES) == SAVE_INVALID);
    assert(Profile_Encode(&profile, 3, bytes));
    bytes[12] |= 0x80; checksum(bytes);
    assert(Profile_Decode(&decoded, &generation, bytes, PROFILE_BYTES) == SAVE_INVALID);
    assert(Profile_Encode(&profile, 3, bytes));
    bytes[23] = 0x80; checksum(bytes); // out-of-range signed score
    assert(Profile_Decode(&decoded, &generation, bytes, PROFILE_BYTES) == SAVE_INVALID);
    assert(Profile_Encode(&profile, 3, bytes));
    memset(bytes + 16, 0, 4); checksum(bytes); // unsorted score list
    assert(Profile_Decode(&decoded, &generation, bytes, PROFILE_BYTES) == SAVE_INVALID);
}
static void test_transactions(void)
{
    Disk disk = {0}; SaveIO io = io_for(&disk); ProfileStore store, loaded;
    Profile_Load(&store, &io);
    assert(store.active_slot == -1 && store.status == SAVE_READY);
    store.profile.inverted = true; store.dirty = true;
    assert(Profile_Save(&store, &io));
    assert(store.active_slot == 0 && store.generation == 1 && !store.dirty);
    store.profile.volume = 4; store.dirty = true;
    assert(Profile_Save(&store, &io));
    assert(store.active_slot == 1 && store.generation == 2);
    Profile_Load(&loaded, &io);
    assert(loaded.profile.inverted && loaded.profile.volume == 4);
    Disk healthy = disk;
    for (int failure = 2; failure <= 5; ++failure) {
        disk = healthy; disk.fail = failure;
        Profile_Load(&store, &io);
        store.profile.volume = 1; store.dirty = true;
        assert(!Profile_Save(&store, &io) && store.dirty && store.status == SAVE_FAILED);
        assert(memcmp(&disk.files[1], &healthy.files[1], sizeof(File)) == 0);
        disk.fail = 0;
        Profile_Load(&loaded, &io);
        assert(loaded.profile.volume == 4 && loaded.generation == 2);
        assert(Profile_Save(&store, &io)); // explicit retry recovers
        Profile_Load(&loaded, &io);
        assert(loaded.profile.volume == 1 && loaded.generation == 3);
    }
    disk = healthy; disk.files[1].data[17] ^= 1;
    Profile_Load(&loaded, &io);
    assert(loaded.status == SAVE_RECOVERED && loaded.generation == 1 && loaded.profile.volume == 2);
    disk.files[0].size = 5;
    Profile_Load(&loaded, &io);
    assert(loaded.status == SAVE_RECOVERED && loaded.active_slot == -1 && !loaded.profile.inverted);
    disk = healthy; disk.files[1].data[4] = 3;
    Profile_Load(&loaded, &io);
    assert(loaded.status == SAVE_READ_ONLY && loaded.generation == 1);
    loaded.dirty = true;
    unsigned writes = disk.writes;
    assert(!Profile_Save(&loaded, &io) && writes == disk.writes);
    disk = healthy; disk.fail = 1;
    Profile_Load(&loaded, &io);
    assert(loaded.status == SAVE_READ_ONLY);
    disk = healthy;
    Profile profile; Profile_Defaults(&profile);
    Profile_Encode(&profile, UINT32_MAX, disk.files[0].data);
    profile.volume = 4;
    Profile_Encode(&profile, 0, disk.files[1].data);
    Profile_Load(&loaded, &io);
    assert(loaded.active_slot == 1 && loaded.profile.volume == 4 && loaded.generation == 0);
}
static void test_daily_and_migration(void)
{
    // Frozen 0.3/v1 fixture, not constructed with the new encoder.
    static const uint8_t v1[PROFILE_V1_BYTES] = {
        0x53,0x57,0x50,0x44,0x01,0x00,0x3c,0x00,0x05,0x00,0x00,0x00,
        0x0d,0x03,0x00,0x00,0x84,0x03,0x00,0x00,0xf4,0x01,0x00,0x00,
        0x64,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x4b,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xba,0x48,0xab,0x88
    };
    Disk disk = {0}; SaveIO io = io_for(&disk); ProfileStore store, loaded;
    memcpy(disk.files[0].data, v1, sizeof(v1));
    disk.files[0].size = sizeof(v1); disk.files[0].exists = true;
    Profile_Load(&store, &io);
    assert(store.status == SAVE_READY && store.generation == 5 && !store.dirty);
    assert(store.profile.volume == 3 && store.profile.inverted && !store.profile.crank);
    assert(store.profile.scores[0][0] == 900 && store.profile.scores[1][0] == 75);
    assert(store.profile.daily_day == PROFILE_NO_DAY);
    assert(Profile_Scores(&store.profile, 0, true, 86399)[0] == 0);
    assert(!Profile_RecordScore(&store.profile, 0, 0, 86399));
    assert(!Profile_RecordScore(&store.profile, 0, -5, 86399));
    assert(!Profile_RecordScore(&store.profile, 2, 100, 86399));
    assert(store.profile.daily_day == PROFILE_NO_DAY);
    assert(Profile_RecordScore(&store.profile, 0, 400, 86399));
    assert(Profile_RecordScore(&store.profile, 1, 50, 86399));
    assert(Profile_Scores(&store.profile, 0, true, 86399)[0] == 400);
    assert(Profile_Scores(&store.profile, 1, true, 86399)[0] == 50);
    assert(Profile_Scores(&store.profile, 0, false, 86399)[0] == 900);
    store.dirty = true;
    assert(Profile_Save(&store, &io) && store.generation == 6);
    assert(memcmp(disk.files[0].data, v1, sizeof(v1)) == 0); // old active survives migration
    assert(disk.files[1].size == PROFILE_BYTES && disk.files[1].data[4] == 2);
    Profile_Load(&loaded, &io);
    assert(loaded.profile.daily_day == 0 && loaded.profile.daily_scores[0][0] == 400);
    // Viewing at the UTC boundary hides yesterday without any file write.
    assert(Profile_Scores(&loaded.profile, 0, true, 86400)[0] == 0);
    assert(Profile_Scores(&loaded.profile, 1, true, 86400)[0] == 0);
    assert(Profile_Scores(&loaded.profile, 0, false, 86400)[0] == 900);
    assert(Profile_RecordScore(&loaded.profile, 0, 200, 86400));
    assert(loaded.profile.daily_day == 1 && loaded.profile.daily_scores[0][0] == 200);
    assert(loaded.profile.daily_scores[1][0] == 0);
    assert(Profile_RecordScore(&loaded.profile, 0, 1000, 86401));
    assert(loaded.profile.scores[0][0] == 1000 && loaded.profile.daily_scores[0][0] == 1000);
    // Clock adjustment: local daily follows the current date; all-time survives.
    assert(Profile_Scores(&loaded.profile, 0, true, 86399)[0] == 0);
    assert(Profile_RecordScore(&loaded.profile, 1, 90, 86399));
    assert(loaded.profile.daily_day == 0 && loaded.profile.daily_scores[0][0] == 0);
    assert(loaded.profile.scores[0][0] == 1000);
    uint8_t bytes[PROFILE_BYTES]; uint32_t generation; Profile decoded;
    assert(Profile_Encode(&loaded.profile, 7, bytes));
    memset(bytes + 56, 0xff, 4); checksum(bytes); // no date with populated daily scores
    assert(Profile_Decode(&decoded, &generation, bytes, sizeof(bytes)) == SAVE_INVALID);
    assert(Profile_Encode(&loaded.profile, 7, bytes));
    bytes[59] = 1; checksum(bytes); // date outside uint32 epoch range
    assert(Profile_Decode(&decoded, &generation, bytes, sizeof(bytes)) == SAVE_INVALID);
    assert(Profile_Encode(&loaded.profile, 7, bytes));
    bytes[83] = 0x80; checksum(bytes); // overflowing daily score
    assert(Profile_Decode(&decoded, &generation, bytes, sizeof(bytes)) == SAVE_INVALID);
    // A broken migrated slot falls back to the untouched v1 profile.
    disk.files[1].data[70] ^= 1;
    Profile_Load(&loaded, &io);
    assert(loaded.status == SAVE_RECOVERED && loaded.generation == 5);
    assert(loaded.profile.scores[0][0] == 900 && loaded.profile.daily_day == PROFILE_NO_DAY);
}
static void test_tone(void)
{
    assert(PortTone_Step(0) == 0 && PortTone_Step(1) == 0 && PortTone_Step(54) == 0);
    assert(PortTone_Step(55) != 0 && PortTone_Step(65535) != 0 && PortTone_Step(65536) == 0);
    PortTone tone = {0}; int16_t left[4410], right[4410];
    uint32_t step = PortTone_Step(1193); // approximately 1000 Hz
    assert(PortTone_Render(&tone, step, 1600, left, right, 4410));
    int crossings = 0;
    for (int i = 0; i < 4410; ++i) {
        assert(left[i] == right[i] && left[i] >= -1600 && left[i] <= 1600);
        if (i && (left[i] > 0) != (left[i-1] > 0)) ++crossings;
    }
    assert(crossings >= 199 && crossings <= 201);
    PortTone_Render(&tone, 0, 3200, left, NULL, 4410);
    assert(tone.gain == 0);
    for (int i = 400; i < 4410; ++i) assert(left[i] == 0);
    assert(!PortTone_Render(&tone, 0, 3200, left, NULL, 4410));
    PortTone_Render(&tone, step, UINT_MAX, left, NULL, 4410);
    assert(tone.gain == 3200);
    PortTone_Render(&tone, step, 0, left, NULL, 4410);
    assert(tone.gain == 0);
}
static void test_markers(void)
{
    uint8_t data[52 * 240 + 2]; memset(data, 0xff, sizeof(data));
    data[0] = data[sizeof(data)-1] = 0x55;
    PortMarker markers[] = {{100,50,MARKER_FRIEND}, {130,50,MARKER_ENEMY}, {160,50,MARKER_PLAYER},
        {0,0,MARKER_FRIEND}, {INT_MAX,INT_MIN,MARKER_ENEMY}, {200,80,(MarkerKind)99}};
    Port_DrawMarkers(data+1, markers, 6, 1);
    assert(data[0] == 0x55 && data[sizeof(data)-1] == 0x55);
    assert(data[1 + 50*52 + 100/8] & (0x80 >> (100&7))); // friendly center white
    assert(!(data[1 + 50*52 + 130/8] & (0x80 >> (130&7)))); // enemy center black
    memset(data+1, 0xff, 52*240);
    Port_DrawMarkers(data+1, markers, 6, 0);
    assert(data[1 + 50*52 + 130/8] == 0xff); // factions can be disabled
    assert(!(data[1 + 51*52 + 160/8] & (0x80 >> (160&7)))); // player stays visible
}
int main(void)
{
    test_codec(); test_transactions(); test_daily_and_migration(); test_tone(); test_markers();
    puts("PASS: save validation/recovery/failure/wrap, v1 migration, daily/all-time scores and UTC rollover, audio bounds/ramp and faction markers");
}
