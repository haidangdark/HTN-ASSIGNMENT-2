/**
 * @file    wm_fsm.h
 * @brief   Hardware-independent finite state machine of the washing machine.
 *
 * This module contains ONLY the control logic. It never touches hardware:
 *   - inputs  arrive as abstract events  (wm_handle_event)
 *   - time    advances by periodic ticks (wm_tick)
 *   - outputs are queried                 (wm_get_outputs)
 *
 * Because of this separation the very same file is compiled for the
 * microcontroller and for the PC test bench.
 *
 * States
 * ------
 *   STANDBY : credit < 50c, waiting for coins          RLED ON,    BLED OFF
 *   READY   : credit >= 50c, waiting for RUN           RLED OFF,   BLED ON
 *   RUNNING : washing, 30-min timer counting           RLED OFF,   BLED BLINK
 *   PAUSED  : washing paused, timer STILL counting     RLED OFF,   BLED ON
 *   ERROR   : invalid coin / hardware fault            RLED BLINK, BLED OFF
 */
#ifndef WM_FSM_H
#define WM_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "wm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* Types                                                                  */
/* ---------------------------------------------------------------------- */

/** Machine states. */
typedef enum {
    WM_STATE_STANDBY = 0,
    WM_STATE_READY,
    WM_STATE_RUNNING,
    WM_STATE_PAUSED,
    WM_STATE_ERROR,
    WM_STATE_COUNT
} wm_state_t;

/** Input events delivered to the FSM. */
typedef enum {
    WM_EV_NONE = 0,
    WM_EV_COIN_10,          /**< valid 10-cent coin detected              */
    WM_EV_COIN_20,          /**< valid 20-cent coin detected              */
    WM_EV_COIN_50,          /**< valid 50-cent coin detected              */
    WM_EV_COIN_INVALID,     /**< unknown / counterfeit coin detected      */
    WM_EV_BTN_RUN,          /**< RUN button pressed (debounced edge)      */
    WM_EV_BTN_PAUSE,        /**< PAUSE button pressed (debounced edge)    */
    WM_EV_BTN_STOP,         /**< STOP button pressed (debounced edge)     */
    WM_EV_FAULT,            /**< hardware fault detected (door, water...) */
    WM_EV_FAULT_CLEAR,      /**< hardware fault removed                   */
    WM_EV_COUNT
} wm_event_t;

/** Error codes (meaningful when state == WM_STATE_ERROR). */
typedef enum {
    WM_ERR_NONE = 0,
    WM_ERR_INVALID_COIN,    /**< transient: auto-recovers after 3 s       */
    WM_ERR_HW_FAULT         /**< persistent: until WM_EV_FAULT_CLEAR      */
} wm_error_t;

/** How the last washing cycle ended. */
typedef enum {
    WM_FINISH_NONE = 0,     /**< no cycle has finished yet                */
    WM_FINISH_COMPLETED,    /**< 30-minute timer elapsed                  */
    WM_FINISH_FORCE_STOP    /**< STOP pressed twice                       */
} wm_finish_t;

/** Result of wm_handle_event(), useful for logging and testing. */
typedef enum {
    WM_RES_OK = 0,          /**< event accepted and acted upon            */
    WM_RES_IGNORED,         /**< event has no meaning in current state    */
    WM_RES_COIN_REJECTED,   /**< coin returned to the customer            */
    WM_RES_STOP_ARMED,      /**< first STOP registered, waiting for 2nd   */
    WM_RES_INVALID_ARG      /**< NULL context or out-of-range event       */
} wm_result_t;

/** LED driving mode. */
typedef enum {
    WM_LED_OFF = 0,
    WM_LED_ON,
    WM_LED_BLINK
} wm_led_mode_t;

/** Physical output levels to be written to the pins. */
typedef struct {
    bool rled;              /**< red LED level   */
    bool bled;              /**< blue LED level  */
    bool motor;             /**< washing motor   */
} wm_outputs_t;

/** Complete FSM context. Treat as opaque; use the accessor functions. */
typedef struct {
    wm_state_t  state;
    wm_error_t  error;
    uint16_t    credit_cents;       /**< inserted money not yet consumed   */
    uint32_t    remaining_ticks;    /**< wash timer (0 = no wash pending)  */
    uint32_t    error_ticks;        /**< remaining invalid-coin display    */
    bool        stop_armed;         /**< first STOP has been pressed       */
    uint32_t    stop_window_ticks;  /**< time left to press 2nd STOP       */
    uint32_t    blink_ticks;        /**< blink phase counter               */
    bool        blink_on;           /**< current blink phase               */
    wm_finish_t last_finish;
    uint32_t    completed_count;    /**< statistics: completed cycles      */
    uint32_t    force_stop_count;   /**< statistics: forced stops          */
} wm_ctx_t;

/* ---------------------------------------------------------------------- */
/* API                                                                    */
/* ---------------------------------------------------------------------- */

/** Reset the context to power-on state (STANDBY, no credit). */
void         wm_init(wm_ctx_t *ctx);

/** Deliver one input event. Must not be called from more than one context. */
wm_result_t  wm_handle_event(wm_ctx_t *ctx, wm_event_t ev);

/** Advance time by one tick (WM_TICK_MS). */
void         wm_tick(wm_ctx_t *ctx);

/* --- accessors -------------------------------------------------------- */
wm_state_t    wm_get_state(const wm_ctx_t *ctx);
wm_error_t    wm_get_error(const wm_ctx_t *ctx);
uint16_t      wm_get_credit(const wm_ctx_t *ctx);
uint32_t      wm_get_remaining_ticks(const wm_ctx_t *ctx);
uint32_t      wm_get_remaining_seconds(const wm_ctx_t *ctx);
bool          wm_is_stop_armed(const wm_ctx_t *ctx);
wm_finish_t   wm_get_last_finish(const wm_ctx_t *ctx);
wm_led_mode_t wm_get_rled_mode(const wm_ctx_t *ctx);
wm_led_mode_t wm_get_bled_mode(const wm_ctx_t *ctx);
wm_outputs_t  wm_get_outputs(const wm_ctx_t *ctx);

/* --- helpers for logging --------------------------------------------- */
const char  *wm_state_name(wm_state_t s);
const char  *wm_event_name(wm_event_t e);
const char  *wm_error_name(wm_error_t e);
const char  *wm_result_name(wm_result_t r);
const char  *wm_finish_name(wm_finish_t f);

#ifdef __cplusplus
}
#endif

#endif /* WM_FSM_H */
