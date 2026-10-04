// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SOPWITH_WORLD_TRACE_H
#define SOPWITH_WORLD_TRACE_H
static uint32_t hash_world(void)
{
    uint32_t hash = 2166136261u;
    for (OBJECTS *ob = objtop; ob; ob = ob->ob_next) {
        int values[] = {ob->ob_type, ob->ob_state, ob->ob_x, ob->ob_y, ob->ob_dx,
            ob->ob_dy, ob->ob_lx, ob->ob_ly, ob->ob_speed, ob->ob_angle,
            ob->ob_life, ob->ob_rounds, ob->ob_bombs, ob->ob_crashcnt, ob->ob_score.score};
        for (size_t i = 0; i < sizeof(values)/sizeof(values[0]); ++i)
            hash = (hash ^ (uint32_t)values[i]) * 16777619u;
    }
    for (int i = 0; i < currgame->gm_max_x; ++i) hash = (hash ^ ground[i]) * 16777619u;
    return hash;
}

#endif
