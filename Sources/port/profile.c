// SPDX-License-Identifier: GPL-2.0-or-later
#include "profile.h"
#include <limits.h>
#include <string.h>

static const char *const slots[] = {"profile-a.dat", "profile-b.dat"};
static const char temporary[] = "profile.tmp";

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (i * 8));
}
static uint32_t crc32(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}
void Profile_Defaults(Profile *profile)
{
    *profile = (Profile){.sound=true, .crank=true, .markers=true, .volume=2,
                         .daily_day=PROFILE_NO_DAY};
}
static bool valid_scores(const int scores[2][PROFILE_SCORES])
{
    for (unsigned mode = 0; mode < 2; ++mode) {
        int previous = INT_MAX;
        for (unsigned i = 0; i < PROFILE_SCORES; ++i) {
            int score = scores[mode][i];
            if (score < 0 || score > previous) return false;
            previous = score;
        }
    }
    return true;
}
static bool valid(const Profile *profile)
{
    if (profile->volume > 4 || !valid_scores(profile->scores) ||
        !valid_scores(profile->daily_scores)) return false;
    if (profile->daily_day == PROFILE_NO_DAY)
        return !profile->daily_scores[0][0] && !profile->daily_scores[1][0];
    return profile->daily_day <= UINT32_MAX / 86400u;
}
static bool add_score(int scores[PROFILE_SCORES], int score)
{
    for (unsigned i = 0; i < PROFILE_SCORES; ++i) {
        if (score > scores[i]) {
            for (unsigned j = PROFILE_SCORES - 1; j > i; --j)
                scores[j] = scores[j - 1];
            scores[i] = score;
            return true;
        }
    }
    return false;
}
bool Profile_AddScore(Profile *profile, unsigned mode, int score)
{
    return mode < 2 && score > 0 && add_score(profile->scores[mode], score);
}
bool Profile_RecordScore(Profile *profile, unsigned mode, int score, uint32_t epoch_seconds)
{
    if (mode >= 2 || score <= 0) return false;
    bool changed = Profile_AddScore(profile, mode, score);
    uint32_t day = epoch_seconds / 86400u;
    if (profile->daily_day != day) {
        memset(profile->daily_scores, 0, sizeof(profile->daily_scores));
        profile->daily_day = day;
        changed = true;
    }
    // Do not short-circuit when the all-time list changed: update both boards.
    return add_score(profile->daily_scores[mode], score) | changed;
}
const int *Profile_Scores(const Profile *profile, unsigned mode, bool daily, uint32_t epoch_seconds)
{
    static const int empty[PROFILE_SCORES];
    if (mode >= 2 || (daily && profile->daily_day != epoch_seconds / 86400u)) return empty;
    return daily ? profile->daily_scores[mode] : profile->scores[mode];
}
bool Profile_Encode(const Profile *profile, uint32_t generation, uint8_t out[PROFILE_BYTES])
{
    if (!valid(profile)) return false;
    memset(out, 0, PROFILE_BYTES);
    memcpy(out, "SWPD", 4);
    out[4] = 2; // little-endian uint16 version
    out[6] = PROFILE_BYTES; // little-endian uint16 length
    put32(out + 8, generation);
    out[12] = profile->sound | profile->crank << 1 | profile->inverted << 2 | profile->markers << 3;
    out[13] = (uint8_t)profile->volume;
    for (unsigned mode = 0; mode < 2; ++mode)
        for (unsigned i = 0; i < PROFILE_SCORES; ++i)
            put32(out + 16 + 4 * (mode * PROFILE_SCORES + i), (uint32_t)profile->scores[mode][i]);
    put32(out + 56, profile->daily_day);
    for (unsigned mode = 0; mode < 2; ++mode)
        for (unsigned i = 0; i < PROFILE_SCORES; ++i)
            put32(out + 60 + 4 * (mode * PROFILE_SCORES + i), (uint32_t)profile->daily_scores[mode][i]);
    put32(out + 100, crc32(out, 100));
    return true;
}
SaveRead Profile_Decode(Profile *profile, uint32_t *generation, const uint8_t *data, size_t size)
{
    if (size < 8 || memcmp(data, "SWPD", 4)) return SAVE_INVALID;
    unsigned version = data[4] | (unsigned)data[5] << 8;
    // Protect newer files even when this version cannot validate their layout.
    if (version > 2) return SAVE_FUTURE;
    size_t expected = version == 1 ? PROFILE_V1_BYTES : PROFILE_BYTES;
    if ((version != 1 && version != 2) || size != expected || data[6] != expected || data[7] ||
        data[12] > 15 || data[14] || data[15] || get32(data + expected - 4) != crc32(data, expected - 4))
        return SAVE_INVALID;
    Profile decoded = {.sound=(data[12] & 1) != 0, .crank=(data[12] & 2) != 0,
        .inverted=(data[12] & 4) != 0, .markers=(data[12] & 8) != 0, .volume=data[13],
        .daily_day=version == 1 ? PROFILE_NO_DAY : get32(data + 56)};
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (unsigned i = 0; i < PROFILE_SCORES; ++i) {
            uint32_t value = get32(data + 16 + 4 * (mode * PROFILE_SCORES + i));
            if (value > INT_MAX) return SAVE_INVALID;
            decoded.scores[mode][i] = (int)value;
            if (version == 2) {
                value = get32(data + 60 + 4 * (mode * PROFILE_SCORES + i));
                if (value > INT_MAX) return SAVE_INVALID;
                decoded.daily_scores[mode][i] = (int)value;
            }
        }
    }
    if (!valid(&decoded)) return SAVE_INVALID;
    *profile = decoded;
    *generation = get32(data + 8);
    return SAVE_OK;
}
void Profile_Load(ProfileStore *store, const SaveIO *io)
{
    *store = (ProfileStore){.active_slot=-1, .status=SAVE_READY};
    Profile_Defaults(&store->profile);
    for (int slot = 0; slot < 2; ++slot) {
        uint8_t bytes[PROFILE_BYTES + 1];
        size_t size = 0;
        Profile profile;
        uint32_t generation;
        SaveRead result = io->read(io->context, slots[slot], bytes, sizeof(bytes), &size);
        if (result == SAVE_OK) result = Profile_Decode(&profile, &generation, bytes, size);
        if (result == SAVE_FUTURE || result == SAVE_IO_ERROR) store->status = SAVE_READ_ONLY;
        else if (result == SAVE_INVALID && store->status != SAVE_READ_ONLY) store->status = SAVE_RECOVERED;
        if (result != SAVE_OK) continue;
        uint32_t delta = generation - store->generation;
        if (store->active_slot < 0 || (delta && delta < 0x80000000u)) {
            store->profile = profile;
            store->generation = generation;
            store->active_slot = slot;
        }
    }
}
static bool verify(const SaveIO *io, const char *path, const uint8_t expected[PROFILE_BYTES])
{
    uint8_t bytes[PROFILE_BYTES + 1];
    size_t size = 0;
    return io->read(io->context, path, bytes, sizeof(bytes), &size) == SAVE_OK &&
           size == PROFILE_BYTES && memcmp(bytes, expected, PROFILE_BYTES) == 0;
}
bool Profile_Save(ProfileStore *store, const SaveIO *io)
{
    if (store->status == SAVE_READ_ONLY) return false;
    if (!store->dirty) return true;
    uint8_t bytes[PROFILE_BYTES];
    int next = store->active_slot == 0 ? 1 : 0;
    uint32_t generation = store->generation + 1;
    if (!Profile_Encode(&store->profile, generation, bytes) ||
        !io->write(io->context, temporary, bytes, sizeof(bytes)) ||
        !verify(io, temporary, bytes) || !io->rename(io->context, temporary, slots[next]) ||
        !verify(io, slots[next], bytes)) {
        store->status = SAVE_FAILED;
        return false;
    }
    store->active_slot = next;
    store->generation = generation;
    store->dirty = false;
    store->status = SAVE_READY;
    return true;
}
