/**
 * @file    wm_debounce.c
 * @brief   Counter-based debouncer implementation.
 */
#include <stddef.h>
#include "wm_debounce.h"

void wm_debounce_init(wm_debounce_t *db, bool initial_level)
{
    if (db == NULL) {
        return;
    }
    db->stable = initial_level;
    db->count  = 0u;
}

wm_edge_t wm_debounce_update(wm_debounce_t *db, bool raw_level)
{
    if (db == NULL) {
        return WM_EDGE_NONE;
    }
    if (raw_level == db->stable) {
        db->count = 0u;                 /* glitch ended -> restart */
        return WM_EDGE_NONE;
    }
    db->count++;
    if (db->count < WM_BUTTON_DEBOUNCE_TICKS) {
        return WM_EDGE_NONE;
    }
    db->count  = 0u;
    db->stable = raw_level;
    return raw_level ? WM_EDGE_PRESS : WM_EDGE_RELEASE;
}

bool wm_debounce_level(const wm_debounce_t *db)
{
    return (db != NULL) ? db->stable : false;
}
