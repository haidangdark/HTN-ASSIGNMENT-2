/**
 * @file    wm_app.c
 * @brief   Application layer implementation.
 */
#include <stddef.h>
#include "wm_app.h"

/* Order in which simultaneous button presses are processed. STOP first so
 * a safety request is never delayed by another button in the same tick. */
static const wm_button_id_t k_btn_order[WM_BTN_COUNT] = {
    WM_BTN_STOP, WM_BTN_PAUSE, WM_BTN_RUN
};

static wm_event_t button_event(wm_button_id_t id)
{
    switch (id) {
    case WM_BTN_STOP:  return WM_EV_BTN_STOP;
    case WM_BTN_RUN:   return WM_EV_BTN_RUN;
    case WM_BTN_PAUSE: return WM_EV_BTN_PAUSE;
    default:           return WM_EV_NONE;
    }
}

static void dispatch(wm_app_t *app, wm_event_t ev)
{
    wm_state_t  from = wm_get_state(&app->fsm);
    wm_result_t res  = wm_handle_event(&app->fsm, ev);
    if (app->listener != NULL) {
        app->listener(app->listener_user, ev, res, from, wm_get_state(&app->fsm));
    }
}

static void write_outputs(wm_app_t *app, bool force)
{
    wm_outputs_t out = wm_get_outputs(&app->fsm);

    /* Only touch the pins when something changed (saves bus cycles). */
    if (force || (out.rled != app->last_out.rled)) {
        app->hal->write_rled(out.rled);
    }
    if (force || (out.bled != app->last_out.bled)) {
        app->hal->write_bled(out.bled);
    }
    if (force || (out.motor != app->last_out.motor)) {
        app->hal->write_motor(out.motor);
    }
    app->last_out = out;
}

wm_event_t wm_app_coin_to_event(uint16_t cents)
{
    switch (cents) {
    case 0u:  return WM_EV_NONE;
    case 10u: return WM_EV_COIN_10;
    case 20u: return WM_EV_COIN_20;
    case 50u: return WM_EV_COIN_50;
    default:  return WM_EV_COIN_INVALID;
    }
}

bool wm_app_init(wm_app_t *app, const wm_hal_t *hal)
{
    unsigned i;

    if ((app == NULL) || (hal == NULL) ||
        (hal->read_button == NULL) || (hal->read_coin == NULL) ||
        (hal->read_fault == NULL)  || (hal->write_rled == NULL) ||
        (hal->write_bled == NULL)  || (hal->write_motor == NULL)) {
        return false;
    }

    app->hal           = hal;
    app->tick_count    = 0u;
    app->listener      = NULL;
    app->listener_user = NULL;
    wm_init(&app->fsm);

    for (i = 0u; i < (unsigned)WM_BTN_COUNT; i++) {
        wm_debounce_init(&app->btn[i], false);
    }
    wm_debounce_init(&app->fault, false);

    write_outputs(app, true);            /* drive pins to a known state */
    return true;
}

void wm_app_set_listener(wm_app_t *app, wm_event_listener_t cb, void *user)
{
    if (app != NULL) {
        app->listener      = cb;
        app->listener_user = user;
    }
}

void wm_app_tick(wm_app_t *app)
{
    unsigned   i;
    wm_edge_t  edge;
    wm_event_t coin_ev;

    if ((app == NULL) || (app->hal == NULL)) {
        return;
    }

    /* 1) Fault line (highest priority). */
    edge = wm_debounce_update(&app->fault, app->hal->read_fault());
    if (edge == WM_EDGE_PRESS) {
        dispatch(app, WM_EV_FAULT);
    } else if (edge == WM_EDGE_RELEASE) {
        dispatch(app, WM_EV_FAULT_CLEAR);
    }

    /* 2) Coin acceptor. */
    coin_ev = wm_app_coin_to_event(app->hal->read_coin());
    if (coin_ev != WM_EV_NONE) {
        dispatch(app, coin_ev);
    }

    /* 3) Buttons: one event per debounced press edge. */
    for (i = 0u; i < (unsigned)WM_BTN_COUNT; i++) {
        wm_button_id_t id = k_btn_order[i];
        edge = wm_debounce_update(&app->btn[id], app->hal->read_button(id));
        if (edge == WM_EDGE_PRESS) {
            dispatch(app, button_event(id));
        }
    }

    /* 4) Time base. */
    wm_tick(&app->fsm);
    app->tick_count++;

    /* 5) Outputs. */
    write_outputs(app, false);
}
