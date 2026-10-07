/**
 * @file    wm_debounce.h
 * @brief   Counter-based debouncer for push buttons and digital inputs.
 *
 * The raw pin is sampled once per tick. The debounced (stable) level only
 * changes after the raw level has been different from it for
 * WM_BUTTON_DEBOUNCE_TICKS consecutive samples. Shorter glitches are ignored.
 */
#ifndef WM_DEBOUNCE_H
#define WM_DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>
#include "wm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WM_EDGE_NONE = 0,
    WM_EDGE_PRESS,      /**< stable level changed released -> pressed */
    WM_EDGE_RELEASE     /**< stable level changed pressed -> released */
} wm_edge_t;

typedef struct {
    bool    stable;     /**< debounced level (true = active/pressed) */
    uint8_t count;      /**< consecutive samples differing from stable */
} wm_debounce_t;

void      wm_debounce_init(wm_debounce_t *db, bool initial_level);
wm_edge_t wm_debounce_update(wm_debounce_t *db, bool raw_level);
bool      wm_debounce_level(const wm_debounce_t *db);

#ifdef __cplusplus
}
#endif

#endif /* WM_DEBOUNCE_H */
