/**
 * @file    wm_app.h
 * @brief   Application layer: glues HAL <-> debouncers <-> FSM.
 *
 * Call wm_app_tick() exactly once every WM_TICK_MS milliseconds. Each call:
 *   1. samples all inputs through the HAL,
 *   2. debounces buttons and the fault line, converts edges into events,
 *   3. feeds the events to the FSM,
 *   4. advances the FSM time base,
 *   5. writes LED/motor outputs through the HAL.
 */
#ifndef WM_APP_H
#define WM_APP_H

#include "wm_fsm.h"
#include "wm_hal.h"
#include "wm_debounce.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Optional observer, called after every event processed by the FSM. */
typedef void (*wm_event_listener_t)(void *user, wm_event_t ev, wm_result_t res,
                                    wm_state_t from, wm_state_t to);

typedef struct {
    wm_ctx_t             fsm;
    const wm_hal_t      *hal;
    wm_debounce_t        btn[WM_BTN_COUNT];
    wm_debounce_t        fault;
    wm_outputs_t         last_out;
    uint32_t             tick_count;
    wm_event_listener_t  listener;
    void                *listener_user;
} wm_app_t;

/** Returns false if app/hal is NULL or the HAL table is incomplete. */
bool        wm_app_init(wm_app_t *app, const wm_hal_t *hal);
void        wm_app_set_listener(wm_app_t *app, wm_event_listener_t cb, void *user);
void        wm_app_tick(wm_app_t *app);

/** Translate a coin value (cents) reported by the acceptor into an event. */
wm_event_t  wm_app_coin_to_event(uint16_t cents);

#ifdef __cplusplus
}
#endif

#endif /* WM_APP_H */
