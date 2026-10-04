// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_PROFILE_H
#define SOPWITH_PROFILE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROFILE_V1_BYTES 60
#define PROFILE_BYTES 104
#define PROFILE_SCORES 5
#define PROFILE_NO_DAY UINT32_MAX
typedef struct {
    bool sound, crank, inverted, markers;
    unsigned volume; // 0..4; independent of the mute switch
    int scores[2][PROFILE_SCORES]; // Dogfight, Practice; completed mission scores
    uint32_t daily_day; // UTC days since the Playdate epoch, or PROFILE_NO_DAY
    int daily_scores[2][PROFILE_SCORES];
} Profile;
typedef enum { SAVE_OK, SAVE_MISSING, SAVE_INVALID, SAVE_FUTURE, SAVE_IO_ERROR } SaveRead;
typedef enum { SAVE_READY, SAVE_RECOVERED, SAVE_READ_ONLY, SAVE_FAILED } SaveStatus;
typedef struct {
    void *context;
    SaveRead (*read)(void *context, const char *path, uint8_t *data, size_t capacity, size_t *size);
    bool (*write)(void *context, const char *path, const uint8_t *data, size_t size);
    bool (*rename)(void *context, const char *from, const char *to);
} SaveIO;
typedef struct {
    Profile profile;
    uint32_t generation;
    int active_slot;
    bool dirty;
    SaveStatus status;
} ProfileStore;

void Profile_Defaults(Profile *profile);
bool Profile_AddScore(Profile *profile, unsigned mode, int score);
bool Profile_RecordScore(Profile *profile, unsigned mode, int score, uint32_t epoch_seconds);
const int *Profile_Scores(const Profile *profile, unsigned mode, bool daily, uint32_t epoch_seconds);
bool Profile_Encode(const Profile *profile, uint32_t generation, uint8_t out[PROFILE_BYTES]);
SaveRead Profile_Decode(Profile *profile, uint32_t *generation, const uint8_t *data, size_t size);
void Profile_Load(ProfileStore *store, const SaveIO *io);
bool Profile_Save(ProfileStore *store, const SaveIO *io);
#endif
