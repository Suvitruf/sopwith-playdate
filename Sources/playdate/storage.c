// SPDX-License-Identifier: GPL-2.0-or-later
#include "storage.h"
#include <string.h>

typedef struct { const char *name; bool found; } FindFile;
static void file_error(PlaydateAPI *pd, const char *operation, const char *name)
{
    const char *error = pd->file->geterr();
    pd->system->logToConsole("SOPWITH storage %s %s: %s", operation, name,
                             error ? error : "unspecified file error");
}
static void find_file(const char *name, void *userdata)
{
    FindFile *find = userdata;
    if (!strcmp(find->name, name)) find->found = true;
}
static SaveRead read_data(void *context, const char *name, uint8_t *data, size_t capacity, size_t *size)
{
    PlaydateAPI *pd = context;
    FindFile find = {.name=name};
    // SDK open/geterr has no structured ENOENT. Enumerate to distinguish a new
    // profile from a read failure, which must not authorize replacing a save.
    if (pd->file->listfiles("/", find_file, &find, 0) < 0) {
        file_error(pd, "list", name); return SAVE_IO_ERROR;
    }
    if (!find.found) return SAVE_MISSING;
    SDFile *file = pd->file->open(name, kFileReadData);
    if (!file) { file_error(pd, "open/read", name); return SAVE_IO_ERROR; }
    *size = 0;
    bool failed = false;
    while (*size < capacity) {
        int n = pd->file->read(file, data + *size, (unsigned)(capacity - *size));
        if (n < 0) { file_error(pd, "read", name); failed = true; break; }
        if (!n) break;
        *size += (size_t)n;
    }
    if (pd->file->close(file) < 0) { file_error(pd, "close/read", name); failed = true; }
    return failed ? SAVE_IO_ERROR : SAVE_OK;
}
static bool write_data(void *context, const char *name, const uint8_t *data, size_t size)
{
    PlaydateAPI *pd = context;
    SDFile *file = pd->file->open(name, kFileWrite);
    if (!file) { file_error(pd, "open/write", name); return false; }
    bool ok = pd->file->write(file, data, (unsigned)size) == (int)size;
    if (!ok) file_error(pd, "write", name);
    if (ok && pd->file->flush(file) < 0) { file_error(pd, "flush", name); ok = false; }
    if (pd->file->close(file) < 0) { file_error(pd, "close/write", name); ok = false; }
    return ok;
}
static bool rename_data(void *context, const char *from, const char *to)
{
    PlaydateAPI *pd = context;
    // Profile_Save has verified the temporary file and selected the INACTIVE
    // slot. Older device firmware refuses rename over an existing file, unlike
    // the documented API and Linux Simulator. Keep the active slot untouched.
    if (strcmp(from, "profile.tmp") ||
        (strcmp(to, "profile-a.dat") && strcmp(to, "profile-b.dat"))) return false;
    size_t unused;
    // Open explicitly from Data, not a merged stat result from a bundled asset.
    SaveRead exists = read_data(pd, to, NULL, 0, &unused);
    if (exists == SAVE_IO_ERROR) return false;
    if (exists == SAVE_OK && pd->file->unlink(to, 0) < 0) {
        file_error(pd, "unlink inactive", to); return false;
    }
    if (pd->file->rename(from, to) < 0) {
        file_error(pd, "rename", to); return false;
    }
    return true;
}
SaveIO Playdate_SaveIO(PlaydateAPI *pd)
{
    return (SaveIO){.context=pd, .read=read_data, .write=write_data, .rename=rename_data};
}
