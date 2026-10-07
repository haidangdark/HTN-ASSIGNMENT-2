/**
 * @file    wm_fsm.c
 * @brief   Implementation of the washing machine finite state machine.
 *
 * Design rules
 *  - No dynamic memory, no floating point, no blocking calls.
 *  - Every function executes in bounded (constant) time -> safe to call
 *    from a periodic timer interrupt or a cooperative scheduler.
 *  - All state lives inside wm_ctx_t, so several instances can coexist
 *    (handy for unit tests).
 */
#include <stddef.h>
#include "wm_fsm.h"

/* ====================================================================== */
/* Private helpers                                                        */
/* ====================================================================== */

/** Change state; restart the blink generator so blinking starts with ON. */
static void enter_state(wm_ctx_t *ctx, wm_state_t next)
{
    if (ctx->state != next) {
        ctx->state       = next;
        ctx->blink_ticks = 0u;
        ctx->blink_on    = true;
    }
}

/** Cancel a pending "STOP twice" sequence. */
static void disarm_stop(wm_ctx_t *ctx)
{
    ctx->stop_armed        = false;
    ctx->stop_window_ticks = 0u;
}

/** Go to STANDBY or READY depending on the current credit. */
static void settle_idle(wm_ctx_t *ctx)
{
    enter_state(ctx, (ctx->credit_cents >= WM_MIN_CREDIT_CENTS)
                     ? WM_STATE_READY : WM_STATE_STANDBY);
}

/** Map a coin event to its value in cents (0 if not a valid coin event). */
static uint16_t coin_value(wm_event_t ev)
{
    switch (ev) {
    case WM_EV_COIN_10: return 10u;
    case WM_EV_COIN_20: return 20u;
    case WM_EV_COIN_50: return 50u;
    default:            return 0u;
    }
}

static bool is_coin_event(wm_event_t ev)
{
    return (ev == WM_EV_COIN_10) || (ev == WM_EV_COIN_20) ||
           (ev == WM_EV_COIN_50) || (ev == WM_EV_COIN_INVALID);
}

/** Add a valid coin to the credit register (rejects on overflow). */
static wm_result_t add_coin(wm_ctx_t *ctx, uint16_t value)
{
    if ((uint32_t)ctx->credit_cents + value > WM_MAX_CREDIT_CENTS) {
        return WM_RES_COIN_REJECTED;              /* coin box full */
    }
    ctx->credit_cents = (uint16_t)(ctx->credit_cents + value);
    return WM_RES_OK;
}

/** READY --RUN--> RUNNING : start the 30 min timer, consume all credit. */
static void start_wash(wm_ctx_t *ctx)
{
    ctx->credit_cents    = 0u;          /* redundancy is NOT returned */
    ctx->remaining_ticks = WM_WASH_TICKS;
    disarm_stop(ctx);
    enter_state(ctx, WM_STATE_RUNNING);
}

/** Terminate the current washing cycle. */
static void finish_wash(wm_ctx_t *ctx, wm_finish_t reason)
{
    ctx->remaining_ticks = 0u;
    ctx->last_finish     = reason;
    if (reason == WM_FINISH_COMPLETED) {
        ctx->completed_count++;
    } else {
        ctx->force_stop_count++;
    }
    disarm_stop(ctx);
    settle_idle(ctx);                   /* normally STANDBY (credit = 0) */
}

/** STOP pressed while RUNNING or PAUSED. */
static wm_result_t handle_stop(wm_ctx_t *ctx)
{
    if (ctx->stop_armed) {
        finish_wash(ctx, WM_FINISH_FORCE_STOP);
        return WM_RES_OK;
    }
    ctx->stop_armed        = true;
    ctx->stop_window_ticks = WM_STOP_WINDOW_TICKS;
    return WM_RES_STOP_ARMED;
}

/* ---------------------------------------------------------------------- */
/* Per-state event handlers                                               */
/* ---------------------------------------------------------------------- */

static wm_result_t on_idle(wm_ctx_t *ctx, wm_event_t ev)   /* STANDBY/READY */
{
    wm_result_t res;

    switch (ev) {
    case WM_EV_COIN_10:
    case WM_EV_COIN_20:
    case WM_EV_COIN_50:
        res = add_coin(ctx, coin_value(ev));
        if (res == WM_RES_OK) {
            settle_idle(ctx);
        }
        return res;

    case WM_EV_COIN_INVALID:
        ctx->error       = WM_ERR_INVALID_COIN;
        ctx->error_ticks = WM_COIN_ERROR_TICKS;
        enter_state(ctx, WM_STATE_ERROR);
        return WM_RES_COIN_REJECTED;

    case WM_EV_BTN_RUN:
        if (ctx->state == WM_STATE_READY) {
            start_wash(ctx);
            return WM_RES_OK;
        }
        return WM_RES_IGNORED;          /* not enough money */

    default:                            /* PAUSE, STOP */
        return WM_RES_IGNORED;
    }
}

static wm_result_t on_running(wm_ctx_t *ctx, wm_event_t ev)
{
    if (is_coin_event(ev)) {
        return WM_RES_COIN_REJECTED;    /* coin slot locked while washing */
    }
    switch (ev) {
    case WM_EV_BTN_PAUSE:
        disarm_stop(ctx);
        enter_state(ctx, WM_STATE_PAUSED);   /* timer keeps counting */
        return WM_RES_OK;
    case WM_EV_BTN_STOP:
        return handle_stop(ctx);
    case WM_EV_BTN_RUN:
        disarm_stop(ctx);               /* breaks a STOP-STOP sequence */
        return WM_RES_IGNORED;
    default:
        return WM_RES_IGNORED;
    }
}

static wm_result_t on_paused(wm_ctx_t *ctx, wm_event_t ev)
{
    if (is_coin_event(ev)) {
        return WM_RES_COIN_REJECTED;
    }
    switch (ev) {
    case WM_EV_BTN_RUN:
        disarm_stop(ctx);
        enter_state(ctx, WM_STATE_RUNNING);  /* resume, timer NOT reset */
        return WM_RES_OK;
    case WM_EV_BTN_STOP:
        return handle_stop(ctx);
    case WM_EV_BTN_PAUSE:
        disarm_stop(ctx);
        return WM_RES_IGNORED;
    default:
        return WM_RES_IGNORED;
    }
}

static wm_result_t on_error(wm_ctx_t *ctx, wm_event_t ev)
{
    wm_result_t res;

    if (ctx->error == WM_ERR_INVALID_COIN) {
        switch (ev) {
        case WM_EV_COIN_10:
        case WM_EV_COIN_20:
        case WM_EV_COIN_50:
            res = add_coin(ctx, coin_value(ev));
            if (res == WM_RES_OK) {     /* a good coin ends the warning */
                ctx->error       = WM_ERR_NONE;
                ctx->error_ticks = 0u;
                settle_idle(ctx);
            }
            return res;
        case WM_EV_COIN_INVALID:
            ctx->error_ticks = WM_COIN_ERROR_TICKS;   /* restart warning */
            return WM_RES_COIN_REJECTED;
        default:
            return WM_RES_IGNORED;      /* buttons ignored while blinking */
        }
    }

    /* WM_ERR_HW_FAULT: machine is locked */
    return is_coin_event(ev) ? WM_RES_COIN_REJECTED : WM_RES_IGNORED;
}

/* ====================================================================== */
/* Public API                                                             */
/* ====================================================================== */

void wm_init(wm_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->state             = WM_STATE_STANDBY;
    ctx->error             = WM_ERR_NONE;
    ctx->credit_cents      = 0u;
    ctx->remaining_ticks   = 0u;
    ctx->error_ticks       = 0u;
    ctx->stop_armed        = false;
    ctx->stop_window_ticks = 0u;
    ctx->blink_ticks       = 0u;
    ctx->blink_on          = true;
    ctx->last_finish       = WM_FINISH_NONE;
    ctx->completed_count   = 0u;
    ctx->force_stop_count  = 0u;
}

wm_result_t wm_handle_event(wm_ctx_t *ctx, wm_event_t ev)
{
    if ((ctx == NULL) || (ev <= WM_EV_NONE) || (ev >= WM_EV_COUNT)) {
        return WM_RES_INVALID_ARG;
    }

    /* --- Hardware fault has the highest priority in every state ------- */
    if (ev == WM_EV_FAULT) {
        if ((ctx->state == WM_STATE_ERROR) && (ctx->error == WM_ERR_HW_FAULT)) {
            return WM_RES_IGNORED;
        }
        ctx->error       = WM_ERR_HW_FAULT;
        ctx->error_ticks = 0u;
        disarm_stop(ctx);
        enter_state(ctx, WM_STATE_ERROR);    /* motor off, timer frozen */
        return WM_RES_OK;
    }
    if (ev == WM_EV_FAULT_CLEAR) {
        if ((ctx->state != WM_STATE_ERROR) || (ctx->error != WM_ERR_HW_FAULT)) {
            return WM_RES_IGNORED;
        }
        ctx->error = WM_ERR_NONE;
        if (ctx->remaining_ticks > 0u) {
            enter_state(ctx, WM_STATE_PAUSED); /* user must press RUN again */
        } else {
            settle_idle(ctx);
        }
        return WM_RES_OK;
    }

    /* --- State dependent behaviour ------------------------------------ */
    switch (ctx->state) {
    case WM_STATE_STANDBY:
    case WM_STATE_READY:   return on_idle(ctx, ev);
    case WM_STATE_RUNNING: return on_running(ctx, ev);
    case WM_STATE_PAUSED:  return on_paused(ctx, ev);
    case WM_STATE_ERROR:   return on_error(ctx, ev);
    default:
        wm_init(ctx);                    /* corrupted state -> safe reset */
        return WM_RES_INVALID_ARG;
    }
}

void wm_tick(wm_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    /* Blink generator (shared by every blinking LED). */
    ctx->blink_ticks++;
    if (ctx->blink_ticks >= WM_BLINK_HALF_TICKS) {
        ctx->blink_ticks = 0u;
        ctx->blink_on    = !ctx->blink_on;
    }

    switch (ctx->state) {
    case WM_STATE_RUNNING:
    case WM_STATE_PAUSED:
        /* STOP confirmation window (0 = no timeout). */
        if (ctx->stop_armed && (WM_STOP_WINDOW_TICKS > 0u)) {
            if (ctx->stop_window_ticks > 0u) {
                ctx->stop_window_ticks--;
            }
            if (ctx->stop_window_ticks == 0u) {
                ctx->stop_armed = false;
            }
        }
        /* Wash timer counts down in RUNNING *and* PAUSED. */
        if (ctx->remaining_ticks > 0u) {
            ctx->remaining_ticks--;
        }
        if (ctx->remaining_ticks == 0u) {
            finish_wash(ctx, WM_FINISH_COMPLETED);
        }
        break;

    case WM_STATE_ERROR:
        if (ctx->error == WM_ERR_INVALID_COIN) {
            if (ctx->error_ticks > 0u) {
                ctx->error_ticks--;
            }
            if (ctx->error_ticks == 0u) {
                ctx->error = WM_ERR_NONE;
                settle_idle(ctx);
            }
        }
        /* WM_ERR_HW_FAULT: everything frozen until the fault is cleared */
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------------------- */
/* Accessors                                                              */
/* ---------------------------------------------------------------------- */

wm_state_t wm_get_state(const wm_ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->state : WM_STATE_ERROR;
}

wm_error_t wm_get_error(const wm_ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->error : WM_ERR_NONE;
}

uint16_t wm_get_credit(const wm_ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->credit_cents : 0u;
}

uint32_t wm_get_remaining_ticks(const wm_ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->remaining_ticks : 0u;
}

uint32_t wm_get_remaining_seconds(const wm_ctx_t *ctx)
{
    if (ctx == NULL) {
        return 0u;
    }
    /* round up so that "0 s" is only shown when the wash is over */
    return (ctx->remaining_ticks * WM_TICK_MS + 999u) / 1000u;
}

bool wm_is_stop_armed(const wm_ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->stop_armed : false;
}

wm_finish_t wm_get_last_finish(const wm_ctx_t *ctx)
{
    return (ctx != NULL) ? ctx->last_finish : WM_FINISH_NONE;
}

wm_led_mode_t wm_get_rled_mode(const wm_ctx_t *ctx)
{
    if (ctx == NULL) {
        return WM_LED_OFF;
    }
    switch (ctx->state) {
    case WM_STATE_STANDBY: return WM_LED_ON;     /* available to serve */
    case WM_STATE_ERROR:   return WM_LED_BLINK;  /* error              */
    default:               return WM_LED_OFF;
    }
}

wm_led_mode_t wm_get_bled_mode(const wm_ctx_t *ctx)
{
    if (ctx == NULL) {
        return WM_LED_OFF;
    }
    switch (ctx->state) {
    case WM_STATE_READY:   return WM_LED_ON;     /* ready to execute   */
    case WM_STATE_PAUSED:  return WM_LED_ON;     /* ready to re-execute*/
    case WM_STATE_RUNNING: return WM_LED_BLINK;  /* washing            */
    default:               return WM_LED_OFF;
    }
}

static bool led_level(const wm_ctx_t *ctx, wm_led_mode_t mode)
{
    return (mode == WM_LED_ON) || ((mode == WM_LED_BLINK) && ctx->blink_on);
}

wm_outputs_t wm_get_outputs(const wm_ctx_t *ctx)
{
    wm_outputs_t out = { false, false, false };
    if (ctx != NULL) {
        out.rled  = led_level(ctx, wm_get_rled_mode(ctx));
        out.bled  = led_level(ctx, wm_get_bled_mode(ctx));
        out.motor = (ctx->state == WM_STATE_RUNNING);
    }
    return out;
}

/* ---------------------------------------------------------------------- */
/* Names                                                                  */
/* ---------------------------------------------------------------------- */

const char *wm_state_name(wm_state_t s)
{
    static const char *const names[WM_STATE_COUNT] = {
        "STANDBY", "READY", "RUNNING", "PAUSED", "ERROR"
    };
    return ((unsigned)s < (unsigned)WM_STATE_COUNT) ? names[s] : "UNKNOWN";
}

const char *wm_event_name(wm_event_t e)
{
    static const char *const names[WM_EV_COUNT] = {
        "NONE", "COIN_10", "COIN_20", "COIN_50", "COIN_INVALID",
        "BTN_RUN", "BTN_PAUSE", "BTN_STOP", "FAULT", "FAULT_CLEAR"
    };
    return ((unsigned)e < (unsigned)WM_EV_COUNT) ? names[e] : "UNKNOWN";
}

const char *wm_error_name(wm_error_t e)
{
    switch (e) {
    case WM_ERR_NONE:         return "NONE";
    case WM_ERR_INVALID_COIN: return "INVALID_COIN";
    case WM_ERR_HW_FAULT:     return "HW_FAULT";
    default:                  return "UNKNOWN";
    }
}

const char *wm_result_name(wm_result_t r)
{
    switch (r) {
    case WM_RES_OK:            return "OK";
    case WM_RES_IGNORED:       return "IGNORED";
    case WM_RES_COIN_REJECTED: return "COIN_REJECTED";
    case WM_RES_STOP_ARMED:    return "STOP_ARMED";
    case WM_RES_INVALID_ARG:   return "INVALID_ARG";
    default:                   return "UNKNOWN";
    }
}

const char *wm_finish_name(wm_finish_t f)
{
    switch (f) {
    case WM_FINISH_NONE:       return "NONE";
    case WM_FINISH_COMPLETED:  return "COMPLETED";
    case WM_FINISH_FORCE_STOP: return "FORCE_STOP";
    default:                   return "UNKNOWN";
    }
}
