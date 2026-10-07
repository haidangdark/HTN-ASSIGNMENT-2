/**
 * @file    wm_config.h
 * @brief   Compile-time configuration of the washing machine controller.
 *
 * Every timing value is expressed in milliseconds and converted to
 * "ticks" of the periodic scheduler (WM_TICK_MS). The controller is a
 * time-triggered system: wm_tick() / wm_app_tick() must be called exactly
 * once every WM_TICK_MS milliseconds (e.g. from a hardware timer ISR flag).
 *
 * All values can be overridden from the compiler command line, e.g.
 *     gcc -DWM_WASH_DURATION_MS=60000UL ...
 */
#ifndef WM_CONFIG_H
#define WM_CONFIG_H

#include <stdint.h>

/* ---------------------------------------------------------------------- */
/* Scheduler                                                              */
/* ---------------------------------------------------------------------- */
#ifndef WM_TICK_MS
#define WM_TICK_MS                  10u          /**< period of one tick   */
#endif

/** Convert milliseconds to number of ticks. */
#define WM_MS_TO_TICKS(ms)          ((uint32_t)((ms) / WM_TICK_MS))

/* ---------------------------------------------------------------------- */
/* Timing                                                                 */
/* ---------------------------------------------------------------------- */
#ifndef WM_WASH_DURATION_MS
#define WM_WASH_DURATION_MS         (30UL * 60UL * 1000UL)   /**< 30 minutes */
#endif

#ifndef WM_BLINK_HALF_PERIOD_MS
#define WM_BLINK_HALF_PERIOD_MS     500u   /**< LED blink: 500 ms ON / 500 ms OFF */
#endif

#ifndef WM_STOP_CONFIRM_WINDOW_MS
#define WM_STOP_CONFIRM_WINDOW_MS   2000u  /**< 2nd STOP must come within 2 s  */
#endif

#ifndef WM_COIN_ERROR_DISPLAY_MS
#define WM_COIN_ERROR_DISPLAY_MS    3000u  /**< invalid-coin error shown for 3 s */
#endif

#ifndef WM_BUTTON_DEBOUNCE_TICKS
#define WM_BUTTON_DEBOUNCE_TICKS    3u     /**< 3 stable samples = 30 ms        */
#endif

#define WM_WASH_TICKS               WM_MS_TO_TICKS(WM_WASH_DURATION_MS)
#define WM_BLINK_HALF_TICKS         WM_MS_TO_TICKS(WM_BLINK_HALF_PERIOD_MS)
#define WM_STOP_WINDOW_TICKS        WM_MS_TO_TICKS(WM_STOP_CONFIRM_WINDOW_MS)
#define WM_COIN_ERROR_TICKS         WM_MS_TO_TICKS(WM_COIN_ERROR_DISPLAY_MS)

/* ---------------------------------------------------------------------- */
/* Money                                                                  */
/* ---------------------------------------------------------------------- */
#ifndef WM_MIN_CREDIT_CENTS
#define WM_MIN_CREDIT_CENTS         50u    /**< credit needed to execute       */
#endif

#ifndef WM_MAX_CREDIT_CENTS
#define WM_MAX_CREDIT_CENTS         990u   /**< credit register saturation     */
#endif

/* ---------------------------------------------------------------------- */
/* Sanity checks                                                          */
/* ---------------------------------------------------------------------- */
#if (WM_BLINK_HALF_PERIOD_MS < WM_TICK_MS)
#error "WM_BLINK_HALF_PERIOD_MS must be >= WM_TICK_MS"
#endif
#if (WM_MAX_CREDIT_CENTS < WM_MIN_CREDIT_CENTS)
#error "WM_MAX_CREDIT_CENTS must be >= WM_MIN_CREDIT_CENTS"
#endif

#endif /* WM_CONFIG_H */
