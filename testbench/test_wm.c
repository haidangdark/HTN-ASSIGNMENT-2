/**
 * @file    test_wm.c
 * @brief   Automated test suite of the washing machine controller.
 *
 * Groups
 *   A. Power-on & LEDs              E. Washing timer (30 min)
 *   B. Coins & credit               F. STOP twice (force stop)
 *   C. RUN / start of washing       G. Hardware fault
 *   D. PAUSE / resume               H. Exhaustive transition table
 *   I. Robustness & random testing  J. Debouncer
 *   K. Full stack (HAL mock + debouncer + FSM)
 *
 * All timing is checked with the REAL configuration (30 min = 180 000 ticks
 * of 10 ms); simulating that many ticks takes only milliseconds on a PC.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "test_framework.h"
#include "wm_fsm.h"
#include "wm_debounce.h"
#include "wm_app.h"

#ifdef _WIN32
#include <windows.h>
#endif

tf_state_t g_tf;

/* ====================================================================== */
/* Helpers                                                                */
/* ====================================================================== */
#define MS(ms)   ((uint32_t)((ms) / WM_TICK_MS))
#define SEC(s)   ((uint32_t)((uint64_t)(s) * 1000u / WM_TICK_MS))
#define MIN(m)   SEC((m) * 60u)

static wm_ctx_t m;      /* machine under test */

static void reset(void)               { wm_init(&m); }
static void ticks(uint32_t n)         { while (n--) { wm_tick(&m); } }
static wm_result_t ev(wm_event_t e)   { return wm_handle_event(&m, e); }
static void make_ready(void)          { reset(); ev(WM_EV_COIN_50); }
static void make_running(void)        { make_ready(); ev(WM_EV_BTN_RUN); }
static void make_paused(void)         { make_running(); ev(WM_EV_BTN_PAUSE); }

#define ASSERT_STATE(s)   TEST_ASSERT_EQ_ENUM((s), wm_get_state(&m), wm_state_name)
#define ASSERT_RES(r, x)  TEST_ASSERT_EQ_ENUM((r), (x), wm_result_name)
#define ASSERT_CREDIT(c)  TEST_ASSERT_EQ_INT((c), wm_get_credit(&m))

/** Check LED modes and motor at once. */
#define ASSERT_IO(rmode, bmode, motor_on)                                 \
    do {                                                                  \
        TEST_ASSERT_EQ_INT((rmode), wm_get_rled_mode(&m));                \
        TEST_ASSERT_EQ_INT((bmode), wm_get_bled_mode(&m));                \
        TEST_ASSERT_EQ_INT((motor_on), wm_get_outputs(&m).motor);         \
    } while (0)

/* ====================================================================== */
/* A. Power-on & LEDs                                                     */
/* ====================================================================== */

static void tc_power_on(void)
{
    reset();
    ASSERT_STATE(WM_STATE_STANDBY);
    ASSERT_CREDIT(0);
    TEST_ASSERT_EQ_INT(0, wm_get_remaining_ticks(&m));
    TEST_ASSERT_EQ_INT(WM_ERR_NONE, wm_get_error(&m));
    TEST_ASSERT_EQ_INT(WM_FINISH_NONE, wm_get_last_finish(&m));
    ASSERT_IO(WM_LED_ON, WM_LED_OFF, false);
    TEST_ASSERT_TRUE(wm_get_outputs(&m).rled);
    TEST_ASSERT_FALSE(wm_get_outputs(&m).bled);
}

static void tc_standby_led_steady(void)
{
    uint32_t i;
    reset();
    for (i = 0; i < SEC(10); i++) {          /* RLED never blinks on standby */
        wm_tick(&m);
        TEST_ASSERT_TRUE(wm_get_outputs(&m).rled);
        TEST_ASSERT_FALSE(wm_get_outputs(&m).bled);
    }
    ASSERT_STATE(WM_STATE_STANDBY);
}

/* ====================================================================== */
/* B. Coins & credit                                                      */
/* ====================================================================== */

static void tc_coin_10(void)
{
    reset();
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_10));
    ASSERT_CREDIT(10);
    ASSERT_STATE(WM_STATE_STANDBY);
    ASSERT_IO(WM_LED_ON, WM_LED_OFF, false);
}

static void tc_coin_20(void)
{
    reset();
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_20));
    ASSERT_CREDIT(20);
    ASSERT_STATE(WM_STATE_STANDBY);
}

static void tc_coin_50_ready(void)
{
    reset();
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_50));
    ASSERT_CREDIT(50);
    ASSERT_STATE(WM_STATE_READY);
    ASSERT_IO(WM_LED_OFF, WM_LED_ON, false);       /* BLED on = ready */
}

static void tc_mix_exact_50(void)
{
    reset();
    ev(WM_EV_COIN_10);
    ev(WM_EV_COIN_20);
    ASSERT_STATE(WM_STATE_STANDBY);
    ev(WM_EV_COIN_20);                              /* 10+20+20 = 50 */
    ASSERT_CREDIT(50);
    ASSERT_STATE(WM_STATE_READY);
}

static void tc_40_not_enough(void)
{
    reset();
    ev(WM_EV_COIN_10); ev(WM_EV_COIN_10); ev(WM_EV_COIN_10); ev(WM_EV_COIN_10);
    ASSERT_CREDIT(40);
    ASSERT_STATE(WM_STATE_STANDBY);
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_RUN));  /* not enough money */
    ASSERT_STATE(WM_STATE_STANDBY);
    ASSERT_CREDIT(40);
    ev(WM_EV_COIN_10);                              /* 50 -> ready */
    ASSERT_STATE(WM_STATE_READY);
}

static void tc_over_50(void)
{
    reset();
    ev(WM_EV_COIN_20); ev(WM_EV_COIN_20);
    ASSERT_STATE(WM_STATE_STANDBY);
    ev(WM_EV_COIN_20);                              /* 60 */
    ASSERT_CREDIT(60);
    ASSERT_STATE(WM_STATE_READY);
}

static void tc_more_coins_in_ready(void)
{
    make_ready();
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_50));
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_10));
    ASSERT_CREDIT(110);
    ASSERT_STATE(WM_STATE_READY);
}

static void tc_invalid_coin_error(void)
{
    reset();
    ev(WM_EV_COIN_20);
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_INVALID));
    ASSERT_STATE(WM_STATE_ERROR);
    TEST_ASSERT_EQ_INT(WM_ERR_INVALID_COIN, wm_get_error(&m));
    ASSERT_CREDIT(20);                              /* credit kept */
    ASSERT_IO(WM_LED_BLINK, WM_LED_OFF, false);     /* RLED blinking */
}

static void tc_invalid_coin_recover_standby(void)
{
    reset();
    ev(WM_EV_COIN_20);
    ev(WM_EV_COIN_INVALID);
    ticks(WM_COIN_ERROR_TICKS - 1u);
    ASSERT_STATE(WM_STATE_ERROR);                   /* still 1 tick to go */
    ticks(1);
    ASSERT_STATE(WM_STATE_STANDBY);                 /* auto recovery after 3 s */
    TEST_ASSERT_EQ_INT(WM_ERR_NONE, wm_get_error(&m));
    ASSERT_CREDIT(20);
    ASSERT_IO(WM_LED_ON, WM_LED_OFF, false);
}

static void tc_invalid_coin_recover_ready(void)
{
    make_ready();
    ev(WM_EV_COIN_INVALID);
    ASSERT_STATE(WM_STATE_ERROR);
    ticks(WM_COIN_ERROR_TICKS);
    ASSERT_STATE(WM_STATE_READY);                   /* back to READY, not STANDBY */
    ASSERT_CREDIT(50);
}

static void tc_valid_coin_ends_error(void)
{
    reset();
    ev(WM_EV_COIN_10); ev(WM_EV_COIN_20);           /* 30 */
    ev(WM_EV_COIN_INVALID);
    ticks(SEC(1));
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_20));       /* 50 */
    ASSERT_STATE(WM_STATE_READY);
    TEST_ASSERT_EQ_INT(WM_ERR_NONE, wm_get_error(&m));
    ASSERT_CREDIT(50);
}

static void tc_repeated_invalid_restarts_error(void)
{
    reset();
    ev(WM_EV_COIN_INVALID);
    ticks(SEC(2));
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_INVALID));  /* restart 3 s */
    ticks(SEC(2));
    ASSERT_STATE(WM_STATE_ERROR);                   /* 4 s total, still error */
    ticks(SEC(1));
    ASSERT_STATE(WM_STATE_STANDBY);
}

static void tc_rled_blink_timing(void)
{
    reset();
    ev(WM_EV_COIN_INVALID);
    TEST_ASSERT_TRUE(wm_get_outputs(&m).rled);      /* starts ON */
    ticks(WM_BLINK_HALF_TICKS - 1u);
    TEST_ASSERT_TRUE(wm_get_outputs(&m).rled);
    ticks(1);
    TEST_ASSERT_FALSE(wm_get_outputs(&m).rled);     /* OFF after 500 ms */
    ticks(WM_BLINK_HALF_TICKS);
    TEST_ASSERT_TRUE(wm_get_outputs(&m).rled);      /* ON after 1000 ms */
    TEST_ASSERT_FALSE(wm_get_outputs(&m).bled);
}

static void tc_buttons_ignored_in_coin_error(void)
{
    make_ready();
    ev(WM_EV_COIN_INVALID);
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_RUN));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_PAUSE));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_ERROR);
    ticks(WM_COIN_ERROR_TICKS);
    ASSERT_STATE(WM_STATE_READY);
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_RUN));       /* works after recovery */
    ASSERT_STATE(WM_STATE_RUNNING);
}

static void tc_credit_saturation(void)
{
    int i;
    reset();
    for (i = 0; i < 19; i++) {
        ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_50));   /* 950 */
    }
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_50));  /* 1000 > 990 */
    ASSERT_CREDIT(950);
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_20));       /* 970 */
    ASSERT_RES(WM_RES_OK, ev(WM_EV_COIN_20));       /* 990 */
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_10));
    ASSERT_CREDIT(WM_MAX_CREDIT_CENTS);
    ASSERT_STATE(WM_STATE_READY);
}

/* ====================================================================== */
/* C. RUN / start of washing                                              */
/* ====================================================================== */

static void tc_run_no_credit(void)
{
    reset();
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_RUN));
    ASSERT_STATE(WM_STATE_STANDBY);
    TEST_ASSERT_EQ_INT(0, wm_get_remaining_ticks(&m));
}

static void tc_run_starts_wash(void)
{
    reset();
    ev(WM_EV_COIN_50); ev(WM_EV_COIN_20);           /* 70 cents */
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_RUN));
    ASSERT_STATE(WM_STATE_RUNNING);
    ASSERT_CREDIT(0);                               /* 20c redundancy NOT returned */
    TEST_ASSERT_EQ_INT(WM_WASH_TICKS, wm_get_remaining_ticks(&m));
    TEST_ASSERT_EQ_INT(30u * 60u, wm_get_remaining_seconds(&m));
    ASSERT_IO(WM_LED_OFF, WM_LED_BLINK, true);
}

static void tc_bled_blink_timing(void)
{
    uint32_t i, toggles = 0;
    bool prev;
    make_running();
    TEST_ASSERT_TRUE(wm_get_outputs(&m).bled);
    ticks(WM_BLINK_HALF_TICKS - 1u);
    TEST_ASSERT_TRUE(wm_get_outputs(&m).bled);
    ticks(1);
    TEST_ASSERT_FALSE(wm_get_outputs(&m).bled);
    ticks(WM_BLINK_HALF_TICKS);
    TEST_ASSERT_TRUE(wm_get_outputs(&m).bled);

    /* 10 s of washing -> 20 toggles (period 1 s), RLED stays off */
    prev = wm_get_outputs(&m).bled;
    for (i = 0; i < SEC(10); i++) {
        wm_tick(&m);
        if (wm_get_outputs(&m).bled != prev) { toggles++; prev = !prev; }
        TEST_ASSERT_FALSE(wm_get_outputs(&m).rled);
    }
    TEST_ASSERT_EQ_INT(20, toggles);
}

static void tc_pause_stop_ignored_when_idle(void)
{
    reset();
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_PAUSE));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_STANDBY);
    make_ready();
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_PAUSE));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_STOP));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_STOP));  /* STOP x2: no refund */
    ASSERT_STATE(WM_STATE_READY);
    ASSERT_CREDIT(50);
}

static void tc_run_while_running_no_reset(void)
{
    make_running();
    ticks(SEC(60));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_RUN));
    ASSERT_STATE(WM_STATE_RUNNING);
    TEST_ASSERT_EQ_INT(WM_WASH_TICKS - SEC(60), wm_get_remaining_ticks(&m));
}

static void tc_coins_rejected_while_washing(void)
{
    make_running();
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_10));
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_20));
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_50));
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_INVALID));
    ASSERT_STATE(WM_STATE_RUNNING);                 /* no error during wash */
    ASSERT_CREDIT(0);
    ev(WM_EV_BTN_PAUSE);
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_50));
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_INVALID));
    ASSERT_STATE(WM_STATE_PAUSED);
    ASSERT_CREDIT(0);
}

/* ====================================================================== */
/* D. PAUSE / resume                                                      */
/* ====================================================================== */

static void tc_pause(void)
{
    make_running();
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_PAUSE));
    ASSERT_STATE(WM_STATE_PAUSED);
    ASSERT_IO(WM_LED_OFF, WM_LED_ON, false);        /* motor off, BLED steady */
}

static void tc_timer_counts_in_pause(void)
{
    make_running();
    ticks(SEC(60));
    ev(WM_EV_BTN_PAUSE);
    ticks(MIN(5));
    ASSERT_STATE(WM_STATE_PAUSED);
    TEST_ASSERT_EQ_INT(WM_WASH_TICKS - SEC(60) - MIN(5), wm_get_remaining_ticks(&m));
}

static void tc_resume(void)
{
    uint32_t before;
    make_paused();
    ticks(SEC(30));
    before = wm_get_remaining_ticks(&m);
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_RUN));
    ASSERT_STATE(WM_STATE_RUNNING);
    TEST_ASSERT_EQ_INT(before, wm_get_remaining_ticks(&m));  /* NOT reset */
    ASSERT_IO(WM_LED_OFF, WM_LED_BLINK, true);
}

static void tc_pause_twice_ignored(void)
{
    make_paused();
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_PAUSE));
    ASSERT_STATE(WM_STATE_PAUSED);
}

static void tc_multi_pause_total_30min(void)
{
    int i;
    make_running();
    for (i = 0; i < 5; i++) {                       /* 5 x (2 min run + 1 min pause) */
        ticks(MIN(2));
        ev(WM_EV_BTN_PAUSE);
        ticks(MIN(1));
        ev(WM_EV_BTN_RUN);
    }
    TEST_ASSERT_EQ_INT(MIN(15), wm_get_remaining_ticks(&m));
    ticks(MIN(15) - 1u);
    ASSERT_STATE(WM_STATE_RUNNING);
    ticks(1);
    ASSERT_STATE(WM_STATE_STANDBY);
    TEST_ASSERT_EQ_INT(WM_FINISH_COMPLETED, wm_get_last_finish(&m));
}

/* ====================================================================== */
/* E. Washing timer                                                       */
/* ====================================================================== */

static void tc_complete_after_30min(void)
{
    make_running();
    ticks(WM_WASH_TICKS - 1u);
    ASSERT_STATE(WM_STATE_RUNNING);                 /* 29:59.99 */
    TEST_ASSERT_EQ_INT(1, wm_get_remaining_ticks(&m));
    TEST_ASSERT_EQ_INT(1, wm_get_remaining_seconds(&m));
    ticks(1);
    ASSERT_STATE(WM_STATE_STANDBY);                 /* exactly 30:00 */
    TEST_ASSERT_EQ_INT(WM_FINISH_COMPLETED, wm_get_last_finish(&m));
    TEST_ASSERT_EQ_INT(1, m.completed_count);
    TEST_ASSERT_EQ_INT(0, wm_get_remaining_ticks(&m));
    ASSERT_IO(WM_LED_ON, WM_LED_OFF, false);
}

static void tc_complete_while_paused(void)
{
    make_paused();
    ticks(WM_WASH_TICKS - 1u);
    ASSERT_STATE(WM_STATE_PAUSED);
    ticks(1);
    ASSERT_STATE(WM_STATE_STANDBY);
    TEST_ASSERT_EQ_INT(WM_FINISH_COMPLETED, wm_get_last_finish(&m));
}

static void tc_new_cycle_after_completion(void)
{
    make_running();
    ticks(WM_WASH_TICKS);
    ASSERT_STATE(WM_STATE_STANDBY);
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_RUN));  /* money was consumed */
    ev(WM_EV_COIN_20); ev(WM_EV_COIN_20); ev(WM_EV_COIN_10);
    ASSERT_STATE(WM_STATE_READY);
    ev(WM_EV_BTN_RUN);
    ASSERT_STATE(WM_STATE_RUNNING);
    TEST_ASSERT_EQ_INT(WM_WASH_TICKS, wm_get_remaining_ticks(&m));
}

/* ====================================================================== */
/* F. STOP twice                                                          */
/* ====================================================================== */

static void tc_single_stop_no_effect(void)
{
    make_running();
    ASSERT_RES(WM_RES_STOP_ARMED, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_RUNNING);
    TEST_ASSERT_TRUE(wm_is_stop_armed(&m));
    TEST_ASSERT_TRUE(wm_get_outputs(&m).motor);
    ticks(WM_STOP_WINDOW_TICKS);
    TEST_ASSERT_FALSE(wm_is_stop_armed(&m));        /* window expired */
    ASSERT_STATE(WM_STATE_RUNNING);
}

static void tc_double_stop_running(void)
{
    make_running();
    ticks(SEC(100));
    ev(WM_EV_BTN_STOP);
    ticks(MS(500));
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_STANDBY);
    TEST_ASSERT_EQ_INT(WM_FINISH_FORCE_STOP, wm_get_last_finish(&m));
    TEST_ASSERT_EQ_INT(1, m.force_stop_count);
    TEST_ASSERT_EQ_INT(0, wm_get_remaining_ticks(&m));
    ASSERT_CREDIT(0);
    ASSERT_IO(WM_LED_ON, WM_LED_OFF, false);
}

static void tc_double_stop_paused(void)
{
    make_paused();
    ASSERT_RES(WM_RES_STOP_ARMED, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_PAUSED);
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_STANDBY);
    TEST_ASSERT_EQ_INT(WM_FINISH_FORCE_STOP, wm_get_last_finish(&m));
}

static void tc_stop_window_expired(void)
{
    make_running();
    ev(WM_EV_BTN_STOP);
    ticks(WM_STOP_WINDOW_TICKS);                    /* > 2 s later ...       */
    ASSERT_RES(WM_RES_STOP_ARMED, ev(WM_EV_BTN_STOP)); /* ... counts as 1st  */
    ASSERT_STATE(WM_STATE_RUNNING);
    ticks(SEC(1));
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_STANDBY);
}

static void tc_stop_window_boundary(void)
{
    make_running();
    ev(WM_EV_BTN_STOP);
    ticks(WM_STOP_WINDOW_TICKS - 1u);               /* last tick of window */
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_STANDBY);
}

static void tc_stop_sequence_interrupted(void)
{
    make_running();
    ev(WM_EV_BTN_STOP);
    ev(WM_EV_BTN_PAUSE);                            /* breaks the sequence */
    TEST_ASSERT_FALSE(wm_is_stop_armed(&m));
    ASSERT_RES(WM_RES_STOP_ARMED, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_PAUSED);
    ev(WM_EV_BTN_RUN);                              /* breaks again */
    ASSERT_RES(WM_RES_STOP_ARMED, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_RUNNING);
    ev(WM_EV_BTN_RUN);                              /* RUN ignored but breaks */
    ASSERT_RES(WM_RES_STOP_ARMED, ev(WM_EV_BTN_STOP));
    ASSERT_RES(WM_RES_OK, ev(WM_EV_BTN_STOP));
    ASSERT_STATE(WM_STATE_STANDBY);
}

static void tc_timer_ends_while_stop_armed(void)
{
    make_running();
    ticks(WM_WASH_TICKS - MS(100));
    ev(WM_EV_BTN_STOP);
    ticks(MS(100));
    ASSERT_STATE(WM_STATE_STANDBY);
    TEST_ASSERT_EQ_INT(WM_FINISH_COMPLETED, wm_get_last_finish(&m));
    TEST_ASSERT_FALSE(wm_is_stop_armed(&m));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_STOP));
}

/* ====================================================================== */
/* G. Hardware fault                                                      */
/* ====================================================================== */

static void tc_fault_in_standby(void)
{
    reset();
    ev(WM_EV_COIN_20);
    ASSERT_RES(WM_RES_OK, ev(WM_EV_FAULT));
    ASSERT_STATE(WM_STATE_ERROR);
    TEST_ASSERT_EQ_INT(WM_ERR_HW_FAULT, wm_get_error(&m));
    ASSERT_IO(WM_LED_BLINK, WM_LED_OFF, false);
    ASSERT_RES(WM_RES_COIN_REJECTED, ev(WM_EV_COIN_50));
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_BTN_RUN));
    ticks(SEC(60));
    ASSERT_STATE(WM_STATE_ERROR);                   /* no auto recovery */
    ASSERT_CREDIT(20);
}

static void tc_fault_clear_idle(void)
{
    reset();
    ev(WM_EV_COIN_20);
    ev(WM_EV_FAULT);
    ASSERT_RES(WM_RES_OK, ev(WM_EV_FAULT_CLEAR));
    ASSERT_STATE(WM_STATE_STANDBY);
    ASSERT_CREDIT(20);
    make_ready();
    ev(WM_EV_COIN_10);
    ev(WM_EV_FAULT);
    ev(WM_EV_FAULT_CLEAR);
    ASSERT_STATE(WM_STATE_READY);
    ASSERT_CREDIT(60);
}

static void tc_fault_while_running(void)
{
    uint32_t frozen;
    make_running();
    ticks(MIN(5));
    ev(WM_EV_FAULT);
    ASSERT_STATE(WM_STATE_ERROR);
    TEST_ASSERT_FALSE(wm_get_outputs(&m).motor);    /* motor stopped at once */
    frozen = wm_get_remaining_ticks(&m);
    TEST_ASSERT_EQ_INT(WM_WASH_TICKS - MIN(5), frozen);
    ticks(MIN(10));
    TEST_ASSERT_EQ_INT(frozen, wm_get_remaining_ticks(&m));  /* timer frozen */
    ev(WM_EV_FAULT_CLEAR);
    ASSERT_STATE(WM_STATE_PAUSED);                  /* safe: wait for RUN */
    ASSERT_IO(WM_LED_OFF, WM_LED_ON, false);
    ev(WM_EV_BTN_RUN);
    ASSERT_STATE(WM_STATE_RUNNING);
    ticks(frozen);
    ASSERT_STATE(WM_STATE_STANDBY);
    TEST_ASSERT_EQ_INT(WM_FINISH_COMPLETED, wm_get_last_finish(&m));
}

static void tc_fault_overrides_coin_error(void)
{
    reset();
    ev(WM_EV_COIN_INVALID);
    ev(WM_EV_FAULT);
    TEST_ASSERT_EQ_INT(WM_ERR_HW_FAULT, wm_get_error(&m));
    ticks(WM_COIN_ERROR_TICKS * 2u);
    ASSERT_STATE(WM_STATE_ERROR);                   /* coin timeout no longer applies */
    ev(WM_EV_FAULT_CLEAR);
    ASSERT_STATE(WM_STATE_STANDBY);
}

static void tc_fault_misc(void)
{
    reset();
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_FAULT_CLEAR));    /* no fault active */
    ev(WM_EV_FAULT);
    ASSERT_RES(WM_RES_IGNORED, ev(WM_EV_FAULT));          /* duplicate */
    ev(WM_EV_FAULT_CLEAR);
    ASSERT_STATE(WM_STATE_STANDBY);

    make_paused();
    ev(WM_EV_BTN_STOP);                                   /* armed */
    ev(WM_EV_FAULT);
    TEST_ASSERT_FALSE(wm_is_stop_armed(&m));              /* disarmed by fault */
    ev(WM_EV_FAULT_CLEAR);
    ASSERT_RES(WM_RES_STOP_ARMED, ev(WM_EV_BTN_STOP));    /* needs 2 again */
    ASSERT_STATE(WM_STATE_PAUSED);
}

/* ====================================================================== */
/* H. Exhaustive transition table: every (state, event) pair              */
/* ====================================================================== */

typedef enum { S_STANDBY, S_READY, S_RUNNING, S_PAUSED, S_ERR_COIN, S_ERR_FAULT } setup_t;

static void setup_state(setup_t s)
{
    reset();
    switch (s) {
    case S_STANDBY:   break;
    case S_READY:     ev(WM_EV_COIN_50); break;
    case S_RUNNING:   ev(WM_EV_COIN_50); ev(WM_EV_BTN_RUN); break;
    case S_PAUSED:    ev(WM_EV_COIN_50); ev(WM_EV_BTN_RUN); ev(WM_EV_BTN_PAUSE); break;
    case S_ERR_COIN:  ev(WM_EV_COIN_INVALID); break;
    case S_ERR_FAULT: ev(WM_EV_FAULT); break;
    }
}

typedef struct { wm_state_t next; wm_result_t res; } expect_t;

#define E(st, r) { WM_STATE_##st, WM_RES_##r }
/* columns: COIN_10, COIN_20, COIN_50, COIN_INVALID, RUN, PAUSE, STOP, FAULT, FAULT_CLEAR */
static const expect_t k_table[6][9] = {
 /* STANDBY  */ { E(STANDBY,OK), E(STANDBY,OK), E(READY,OK), E(ERROR,COIN_REJECTED),
                  E(STANDBY,IGNORED), E(STANDBY,IGNORED), E(STANDBY,IGNORED),
                  E(ERROR,OK), E(STANDBY,IGNORED) },
 /* READY    */ { E(READY,OK), E(READY,OK), E(READY,OK), E(ERROR,COIN_REJECTED),
                  E(RUNNING,OK), E(READY,IGNORED), E(READY,IGNORED),
                  E(ERROR,OK), E(READY,IGNORED) },
 /* RUNNING  */ { E(RUNNING,COIN_REJECTED), E(RUNNING,COIN_REJECTED),
                  E(RUNNING,COIN_REJECTED), E(RUNNING,COIN_REJECTED),
                  E(RUNNING,IGNORED), E(PAUSED,OK), E(RUNNING,STOP_ARMED),
                  E(ERROR,OK), E(RUNNING,IGNORED) },
 /* PAUSED   */ { E(PAUSED,COIN_REJECTED), E(PAUSED,COIN_REJECTED),
                  E(PAUSED,COIN_REJECTED), E(PAUSED,COIN_REJECTED),
                  E(RUNNING,OK), E(PAUSED,IGNORED), E(PAUSED,STOP_ARMED),
                  E(ERROR,OK), E(PAUSED,IGNORED) },
 /* ERR_COIN */ { E(STANDBY,OK), E(STANDBY,OK), E(READY,OK), E(ERROR,COIN_REJECTED),
                  E(ERROR,IGNORED), E(ERROR,IGNORED), E(ERROR,IGNORED),
                  E(ERROR,OK), E(ERROR,IGNORED) },
 /* ERR_FLT  */ { E(ERROR,COIN_REJECTED), E(ERROR,COIN_REJECTED),
                  E(ERROR,COIN_REJECTED), E(ERROR,COIN_REJECTED),
                  E(ERROR,IGNORED), E(ERROR,IGNORED), E(ERROR,IGNORED),
                  E(ERROR,IGNORED), E(STANDBY,OK) },
};
#undef E

static void check_row(setup_t s)
{
    int e;
    for (e = 0; e < 9; e++) {
        wm_event_t  event = (wm_event_t)(WM_EV_COIN_10 + e);
        wm_result_t r;
        setup_state(s);
        r = ev(event);
        if (r != k_table[s][e].res || wm_get_state(&m) != k_table[s][e].next) {
            TF_FAIL_MSG("event %s: expected (%s,%s) got (%s,%s)",
                        wm_event_name(event),
                        wm_state_name(k_table[s][e].next), wm_result_name(k_table[s][e].res),
                        wm_state_name(wm_get_state(&m)), wm_result_name(r));
        }
        g_tf.assertions++;
    }
}

static void tc_table_standby(void)   { check_row(S_STANDBY); }
static void tc_table_ready(void)     { check_row(S_READY); }
static void tc_table_running(void)   { check_row(S_RUNNING); }
static void tc_table_paused(void)    { check_row(S_PAUSED); }
static void tc_table_err_coin(void)  { check_row(S_ERR_COIN); }
static void tc_table_err_fault(void) { check_row(S_ERR_FAULT); }

/* ====================================================================== */
/* I. Robustness                                                          */
/* ====================================================================== */

static void tc_invalid_args(void)
{
    reset();
    ASSERT_RES(WM_RES_INVALID_ARG, wm_handle_event(NULL, WM_EV_BTN_RUN));
    ASSERT_RES(WM_RES_INVALID_ARG, ev(WM_EV_NONE));
    ASSERT_RES(WM_RES_INVALID_ARG, ev(WM_EV_COUNT));
    ASSERT_RES(WM_RES_INVALID_ARG, ev((wm_event_t)99));
    ASSERT_STATE(WM_STATE_STANDBY);
    wm_init(NULL);                                  /* must not crash */
    wm_tick(NULL);
    TEST_ASSERT_EQ_INT(0, wm_get_credit(NULL));
    TEST_ASSERT_FALSE(wm_get_outputs(NULL).motor);

    m.state = (wm_state_t)42;                       /* memory corruption */
    ASSERT_RES(WM_RES_INVALID_ARG, ev(WM_EV_COIN_10));
    ASSERT_STATE(WM_STATE_STANDBY);                 /* safe reset */
}

static void tc_names(void)
{
    TEST_ASSERT(strcmp(wm_state_name(WM_STATE_RUNNING), "RUNNING") == 0);
    TEST_ASSERT(strcmp(wm_state_name((wm_state_t)77), "UNKNOWN") == 0);
    TEST_ASSERT(strcmp(wm_event_name(WM_EV_BTN_STOP), "BTN_STOP") == 0);
    TEST_ASSERT(strcmp(wm_event_name((wm_event_t)77), "UNKNOWN") == 0);
    TEST_ASSERT(strcmp(wm_error_name(WM_ERR_HW_FAULT), "HW_FAULT") == 0);
    TEST_ASSERT(strcmp(wm_result_name(WM_RES_STOP_ARMED), "STOP_ARMED") == 0);
    TEST_ASSERT(strcmp(wm_finish_name(WM_FINISH_FORCE_STOP), "FORCE_STOP") == 0);
}

static uint32_t lcg_state;
static uint32_t rnd(uint32_t n)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return (lcg_state >> 8) % n;
}

static void tc_random_invariants(void)
{
    uint32_t step, visited[WM_STATE_COUNT] = {0};
    lcg_state = 20261007u;                          /* fixed seed: reproducible */
    reset();

    for (step = 0; step < 200000u; step++) {
        uint32_t     r = rnd(100);
        wm_state_t   s;
        wm_led_mode_t rm, bm;
        uint16_t     c;

        if (r < 60) {
            /* random event, faults less frequent */
            wm_event_t e = (wm_event_t)(WM_EV_COIN_10 + rnd(7));
            if (rnd(20) == 0) e = (rnd(2) ? WM_EV_FAULT : WM_EV_FAULT_CLEAR);
            ev(e);
        } else if (r < 99 || rnd(4) != 0) {
            ticks(rnd(400));
        } else {
            ticks(rnd(WM_WASH_TICKS));              /* sometimes a long wait */
        }

        s  = wm_get_state(&m);
        c  = wm_get_credit(&m);
        rm = wm_get_rled_mode(&m);
        bm = wm_get_bled_mode(&m);
        visited[s]++;

        TEST_ASSERT((unsigned)s < (unsigned)WM_STATE_COUNT);
        TEST_ASSERT(c <= WM_MAX_CREDIT_CENTS);
        TEST_ASSERT(wm_get_remaining_ticks(&m) <= WM_WASH_TICKS);
        TEST_ASSERT(((rm != WM_LED_OFF) + (bm != WM_LED_OFF)) == 1); /* exactly 1 LED active */
        TEST_ASSERT(wm_get_outputs(&m).motor == (s == WM_STATE_RUNNING));
        TEST_ASSERT((s == WM_STATE_ERROR) == (wm_get_error(&m) != WM_ERR_NONE));
        if (s == WM_STATE_STANDBY) TEST_ASSERT(c < WM_MIN_CREDIT_CENTS);
        if (s == WM_STATE_READY)   TEST_ASSERT(c >= WM_MIN_CREDIT_CENTS);
        if (s == WM_STATE_RUNNING || s == WM_STATE_PAUSED) {
            TEST_ASSERT(c == 0);
            TEST_ASSERT(wm_get_remaining_ticks(&m) > 0);
        }
        if (wm_is_stop_armed(&m)) {
            TEST_ASSERT(s == WM_STATE_RUNNING || s == WM_STATE_PAUSED);
        }
    }
    for (step = 0; step < WM_STATE_COUNT; step++) {
        TEST_ASSERT(visited[step] > 0);             /* every state explored */
    }
    TEST_ASSERT(m.completed_count > 0);
    TEST_ASSERT(m.force_stop_count > 0);
}

/* ====================================================================== */
/* J. Debouncer                                                           */
/* ====================================================================== */

static void tc_debounce_press(void)
{
    wm_debounce_t db;
    uint32_t i;
    wm_debounce_init(&db, false);
    for (i = 0; i + 1u < WM_BUTTON_DEBOUNCE_TICKS; i++) {
        TEST_ASSERT_EQ_INT(WM_EDGE_NONE, wm_debounce_update(&db, true));
    }
    TEST_ASSERT_EQ_INT(WM_EDGE_PRESS, wm_debounce_update(&db, true));
    TEST_ASSERT_TRUE(wm_debounce_level(&db));
    TEST_ASSERT_EQ_INT(WM_EDGE_NONE, wm_debounce_update(&db, true));
}

static void tc_debounce_glitch(void)
{
    wm_debounce_t db;
    int k;
    uint32_t i;
    wm_debounce_init(&db, false);
    for (k = 0; k < 100; k++) {                     /* bursts shorter than N */
        for (i = 0; i + 1u < WM_BUTTON_DEBOUNCE_TICKS; i++) {
            TEST_ASSERT_EQ_INT(WM_EDGE_NONE, wm_debounce_update(&db, true));
        }
        TEST_ASSERT_EQ_INT(WM_EDGE_NONE, wm_debounce_update(&db, false));
    }
    TEST_ASSERT_FALSE(wm_debounce_level(&db));
}

static void tc_debounce_hold_release(void)
{
    wm_debounce_t db;
    int i, presses = 0, releases = 0;
    wm_debounce_init(&db, false);
    for (i = 0; i < 1000; i++) {                    /* hold for 10 s */
        wm_edge_t e = wm_debounce_update(&db, true);
        presses  += (e == WM_EDGE_PRESS);
        releases += (e == WM_EDGE_RELEASE);
    }
    for (i = 0; i < 100; i++) {
        wm_edge_t e = wm_debounce_update(&db, false);
        presses  += (e == WM_EDGE_PRESS);
        releases += (e == WM_EDGE_RELEASE);
    }
    TEST_ASSERT_EQ_INT(1, presses);                 /* no auto-repeat */
    TEST_ASSERT_EQ_INT(1, releases);
}

/* ====================================================================== */
/* K. Full stack with a mock HAL                                          */
/* ====================================================================== */

static bool     hal_btn[WM_BTN_COUNT];
static uint16_t hal_coin;                           /* one-shot */
static bool     hal_fault;
static bool     pin_rled, pin_bled, pin_motor;
static int      n_run_events, n_stop_events;

static bool     mock_read_button(wm_button_id_t id) { return hal_btn[id]; }
static uint16_t mock_read_coin(void) { uint16_t c = hal_coin; hal_coin = 0; return c; }
static bool     mock_read_fault(void) { return hal_fault; }
static void     mock_rled(bool on)  { pin_rled  = on; }
static void     mock_bled(bool on)  { pin_bled  = on; }
static void     mock_motor(bool on) { pin_motor = on; }

static const wm_hal_t k_mock_hal = {
    mock_read_button, mock_read_coin, mock_read_fault,
    mock_rled, mock_bled, mock_motor
};

static wm_app_t app;

static void listener(void *user, wm_event_t e, wm_result_t r, wm_state_t f, wm_state_t t)
{
    (void)user; (void)r; (void)f; (void)t;
    if (e == WM_EV_BTN_RUN)  n_run_events++;
    if (e == WM_EV_BTN_STOP) n_stop_events++;
}

static void app_reset(void)
{
    memset(hal_btn, 0, sizeof(hal_btn));
    hal_coin = 0; hal_fault = false;
    pin_rled = pin_bled = pin_motor = false;
    n_run_events = n_stop_events = 0;
    (void)wm_app_init(&app, &k_mock_hal);
    wm_app_set_listener(&app, listener, NULL);
}
static void app_ticks(uint32_t n)        { while (n--) wm_app_tick(&app); }
static void app_coin(uint16_t c)         { hal_coin = c; app_ticks(1); }
static void app_press(wm_button_id_t id) /* 80 ms press + 80 ms release */
{
    hal_btn[id] = true;  app_ticks(8);
    hal_btn[id] = false; app_ticks(8);
}
#define APP_STATE()  wm_get_state(&app.fsm)

static void tc_app_init(void)
{
    wm_hal_t broken = k_mock_hal;
    app_reset();
    TEST_ASSERT_TRUE(pin_rled);                     /* pins driven at init */
    TEST_ASSERT_FALSE(pin_bled);
    TEST_ASSERT_FALSE(pin_motor);
    broken.write_motor = NULL;
    TEST_ASSERT_FALSE(wm_app_init(&app, &broken));
    TEST_ASSERT_FALSE(wm_app_init(&app, NULL));
    TEST_ASSERT_FALSE(wm_app_init(NULL, &k_mock_hal));
}

static void tc_app_coin_mapping(void)
{
    TEST_ASSERT_EQ_INT(WM_EV_NONE,         wm_app_coin_to_event(0));
    TEST_ASSERT_EQ_INT(WM_EV_COIN_10,      wm_app_coin_to_event(10));
    TEST_ASSERT_EQ_INT(WM_EV_COIN_20,      wm_app_coin_to_event(20));
    TEST_ASSERT_EQ_INT(WM_EV_COIN_50,      wm_app_coin_to_event(50));
    TEST_ASSERT_EQ_INT(WM_EV_COIN_INVALID, wm_app_coin_to_event(5));
    TEST_ASSERT_EQ_INT(WM_EV_COIN_INVALID, wm_app_coin_to_event(25));
    TEST_ASSERT_EQ_INT(WM_EV_COIN_INVALID, wm_app_coin_to_event(100));
}

static void tc_app_full_cycle(void)
{
    uint32_t i, toggles = 0;
    bool prev;
    app_reset();
    app_coin(20); app_coin(20); app_coin(10);
    TEST_ASSERT_EQ_INT(WM_STATE_READY, APP_STATE());
    TEST_ASSERT_TRUE(pin_bled);
    TEST_ASSERT_FALSE(pin_rled);

    app_press(WM_BTN_RUN);
    TEST_ASSERT_EQ_INT(WM_STATE_RUNNING, APP_STATE());
    TEST_ASSERT_TRUE(pin_motor);
    prev = pin_bled;
    for (i = 0; i < SEC(5); i++) {                  /* BLED pin is toggling */
        wm_app_tick(&app);
        if (pin_bled != prev) { toggles++; prev = pin_bled; }
    }
    TEST_ASSERT(toggles >= 9);
    app_ticks(WM_WASH_TICKS);
    TEST_ASSERT_EQ_INT(WM_STATE_STANDBY, APP_STATE());
    TEST_ASSERT_TRUE(pin_rled);
    TEST_ASSERT_FALSE(pin_bled);
    TEST_ASSERT_FALSE(pin_motor);
}

static void tc_app_bouncing_button(void)
{
    static const bool bounce[] = { 1,0,1,0,0,1,1,0,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,0,1,0,0,0,0,0 };
    size_t i;
    app_reset();
    app_coin(50);
    for (i = 0; i < sizeof(bounce) / sizeof(bounce[0]); i++) {
        hal_btn[WM_BTN_RUN] = bounce[i];
        app_ticks(1);
    }
    TEST_ASSERT_EQ_INT(1, n_run_events);            /* exactly one RUN */
    TEST_ASSERT_EQ_INT(WM_STATE_RUNNING, APP_STATE());
}

static void tc_app_spike_ignored(void)
{
    app_reset();
    app_coin(50);
    hal_btn[WM_BTN_RUN] = true;  app_ticks(1);      /* 10 ms EMI spike */
    hal_btn[WM_BTN_RUN] = false; app_ticks(10);
    TEST_ASSERT_EQ_INT(0, n_run_events);
    TEST_ASSERT_EQ_INT(WM_STATE_READY, APP_STATE());
}

static void tc_app_double_stop(void)
{
    app_reset();
    app_coin(50);
    app_press(WM_BTN_RUN);
    app_ticks(SEC(10));
    app_press(WM_BTN_STOP);
    TEST_ASSERT_EQ_INT(WM_STATE_RUNNING, APP_STATE());
    app_ticks(MS(300));
    app_press(WM_BTN_STOP);
    TEST_ASSERT_EQ_INT(2, n_stop_events);
    TEST_ASSERT_EQ_INT(WM_STATE_STANDBY, APP_STATE());
    TEST_ASSERT_FALSE(pin_motor);
    TEST_ASSERT_TRUE(pin_rled);
}

static void tc_app_pause_resume(void)
{
    app_reset();
    app_coin(50);
    app_press(WM_BTN_RUN);
    app_press(WM_BTN_PAUSE);
    TEST_ASSERT_EQ_INT(WM_STATE_PAUSED, APP_STATE());
    TEST_ASSERT_FALSE(pin_motor);
    TEST_ASSERT_TRUE(pin_bled);
    app_press(WM_BTN_RUN);
    TEST_ASSERT_EQ_INT(WM_STATE_RUNNING, APP_STATE());
    TEST_ASSERT_TRUE(pin_motor);
}

static void tc_app_invalid_coin(void)
{
    uint32_t i, toggles = 0;
    bool prev;
    app_reset();
    app_coin(25);                                   /* not accepted */
    TEST_ASSERT_EQ_INT(WM_STATE_ERROR, APP_STATE());
    prev = pin_rled;
    for (i = 0; i < SEC(2); i++) {
        wm_app_tick(&app);
        if (pin_rled != prev) { toggles++; prev = pin_rled; }
    }
    TEST_ASSERT(toggles >= 3);                      /* RLED pin blinking */
    app_ticks(SEC(1));
    TEST_ASSERT_EQ_INT(WM_STATE_STANDBY, APP_STATE());
    TEST_ASSERT_TRUE(pin_rled);
}

static void tc_app_fault_line(void)
{
    app_reset();
    app_coin(50);
    app_press(WM_BTN_RUN);
    hal_fault = true;  app_ticks(1);                /* glitch, filtered */
    hal_fault = false; app_ticks(5);
    TEST_ASSERT_EQ_INT(WM_STATE_RUNNING, APP_STATE());
    hal_fault = true;  app_ticks(10);               /* real fault */
    TEST_ASSERT_EQ_INT(WM_STATE_ERROR, APP_STATE());
    TEST_ASSERT_FALSE(pin_motor);
    hal_fault = false; app_ticks(10);
    TEST_ASSERT_EQ_INT(WM_STATE_PAUSED, APP_STATE());
}

/* ====================================================================== */
/* Main                                                                   */
/* ====================================================================== */

int main(int argc, char **argv)
{
#ifdef _WIN32
    {   /* enable ANSI colours on the Windows console */
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004u);
    }
#endif
    TF_BEGIN(argc, argv);
    printf("Washing machine controller - automated test suite\n");
    printf("Config: tick=%u ms, wash=%lu ticks (30 min), blink=%u ms, stop window=%u ms\n",
           (unsigned)WM_TICK_MS, (unsigned long)WM_WASH_TICKS,
           (unsigned)WM_BLINK_HALF_PERIOD_MS, (unsigned)WM_STOP_CONFIRM_WINDOW_MS);

    TF_SECTION("A. Power-on & LEDs");
    RUN_TEST("TC01", tc_power_on,                 "Power-on: STANDBY, credit 0, RLED on, BLED off");
    RUN_TEST("TC02", tc_standby_led_steady,       "RLED steady (not blinking) during standby");

    TF_SECTION("B. Coins & credit");
    RUN_TEST("TC03", tc_coin_10,                  "Insert 10c -> credit 10, stay STANDBY");
    RUN_TEST("TC04", tc_coin_20,                  "Insert 20c -> credit 20, stay STANDBY");
    RUN_TEST("TC05", tc_coin_50_ready,            "Insert 50c -> READY, BLED on, RLED off");
    RUN_TEST("TC06", tc_mix_exact_50,             "10+20+20 = 50c reaches READY exactly");
    RUN_TEST("TC07", tc_40_not_enough,            "40c is not enough: RUN ignored");
    RUN_TEST("TC08", tc_over_50,                  "20+20+20 = 60c -> READY");
    RUN_TEST("TC09", tc_more_coins_in_ready,      "Extra coins accumulate while READY");
    RUN_TEST("TC10", tc_invalid_coin_error,       "Invalid coin -> ERROR, RLED blinking, credit kept");
    RUN_TEST("TC11", tc_invalid_coin_recover_standby, "Invalid-coin error auto-clears after 3 s -> STANDBY");
    RUN_TEST("TC12", tc_invalid_coin_recover_ready,   "Invalid-coin error in READY recovers to READY");
    RUN_TEST("TC13", tc_valid_coin_ends_error,    "Valid coin during error is accepted, ends error");
    RUN_TEST("TC14", tc_repeated_invalid_restarts_error, "Repeated invalid coin restarts the 3 s error");
    RUN_TEST("TC15", tc_rled_blink_timing,        "RLED blink timing 500 ms ON / 500 ms OFF");
    RUN_TEST("TC16", tc_buttons_ignored_in_coin_error, "Buttons ignored during error display");
    RUN_TEST("TC17", tc_credit_saturation,        "Credit limited to 990c, extra coin rejected");

    TF_SECTION("C. RUN / start of washing");
    RUN_TEST("TC18", tc_run_no_credit,            "RUN with 0c ignored");
    RUN_TEST("TC19", tc_run_starts_wash,          "RUN in READY: wash 30 min, money cleared (no change)");
    RUN_TEST("TC20", tc_bled_blink_timing,        "BLED blinks 1 Hz while running, RLED off");
    RUN_TEST("TC21", tc_pause_stop_ignored_when_idle, "PAUSE/STOP ignored in STANDBY/READY");
    RUN_TEST("TC22", tc_run_while_running_no_reset,   "RUN while running does not restart timer");
    RUN_TEST("TC23", tc_coins_rejected_while_washing, "Coins rejected while RUNNING/PAUSED");

    TF_SECTION("D. PAUSE / resume");
    RUN_TEST("TC24", tc_pause,                    "PAUSE: motor off, BLED steady on");
    RUN_TEST("TC25", tc_timer_counts_in_pause,    "Timer keeps counting down while paused");
    RUN_TEST("TC26", tc_resume,                   "RUN in PAUSED resumes, timer not reset");
    RUN_TEST("TC27", tc_pause_twice_ignored,      "PAUSE while paused ignored");
    RUN_TEST("TC28", tc_multi_pause_total_30min,  "Many pause/resume cycles: total still 30 min");

    TF_SECTION("E. Washing timer");
    RUN_TEST("TC29", tc_complete_after_30min,     "Wash ends automatically at exactly 30:00");
    RUN_TEST("TC30", tc_complete_while_paused,    "Timer may expire while paused -> STANDBY");
    RUN_TEST("TC31", tc_new_cycle_after_completion, "Machine serves a new customer after finishing");

    TF_SECTION("F. STOP twice (force stop)");
    RUN_TEST("TC32", tc_single_stop_no_effect,    "Single STOP does not stop the machine");
    RUN_TEST("TC33", tc_double_stop_running,      "STOP x2 while RUNNING -> force stop");
    RUN_TEST("TC34", tc_double_stop_paused,       "STOP x2 while PAUSED -> force stop");
    RUN_TEST("TC35", tc_stop_window_expired,      "2nd STOP after 2 s window counts as 1st");
    RUN_TEST("TC36", tc_stop_window_boundary,     "2nd STOP at last tick of window still stops");
    RUN_TEST("TC37", tc_stop_sequence_interrupted,"RUN/PAUSE between STOPs breaks the sequence");
    RUN_TEST("TC38", tc_timer_ends_while_stop_armed, "Timer completes while STOP armed -> COMPLETED");

    TF_SECTION("G. Hardware fault");
    RUN_TEST("TC39", tc_fault_in_standby,         "Fault -> ERROR, RLED blink, coins rejected, no auto clear");
    RUN_TEST("TC40", tc_fault_clear_idle,         "Fault clear returns to STANDBY/READY, credit kept");
    RUN_TEST("TC41", tc_fault_while_running,      "Fault while running: motor off, timer frozen, -> PAUSED");
    RUN_TEST("TC42", tc_fault_overrides_coin_error, "Hardware fault overrides invalid-coin error");
    RUN_TEST("TC43", tc_fault_misc,               "Duplicate fault / spurious clear / STOP disarm");

    TF_SECTION("H. Exhaustive transition table (6 states x 9 events = 54 pairs)");
    RUN_TEST("TC44", tc_table_standby,            "Table row STANDBY (9 events)");
    RUN_TEST("TC45", tc_table_ready,              "Table row READY (9 events)");
    RUN_TEST("TC46", tc_table_running,            "Table row RUNNING (9 events)");
    RUN_TEST("TC47", tc_table_paused,             "Table row PAUSED (9 events)");
    RUN_TEST("TC48", tc_table_err_coin,           "Table row ERROR / invalid coin (9 events)");
    RUN_TEST("TC49", tc_table_err_fault,          "Table row ERROR / hardware fault (9 events)");

    TF_SECTION("I. Robustness");
    RUN_TEST("TC50", tc_invalid_args,             "NULL pointers, bad events, corrupted state");
    RUN_TEST("TC51", tc_names,                    "Name helpers for logging");
    RUN_TEST("TC52", tc_random_invariants,        "200k random steps: all safety invariants hold");

    TF_SECTION("J. Debouncer");
    RUN_TEST("TC53", tc_debounce_press,           "Press detected after N stable samples");
    RUN_TEST("TC54", tc_debounce_glitch,          "Glitches shorter than N samples rejected");
    RUN_TEST("TC55", tc_debounce_hold_release,    "Long hold -> exactly 1 press and 1 release");

    TF_SECTION("K. Full stack (mock HAL + debouncer + FSM)");
    RUN_TEST("TC56", tc_app_init,                 "App init drives pins, rejects incomplete HAL");
    RUN_TEST("TC57", tc_app_coin_mapping,         "Coin acceptor value -> event mapping");
    RUN_TEST("TC58", tc_app_full_cycle,           "Coins -> RUN -> blinking -> 30 min -> STANDBY");
    RUN_TEST("TC59", tc_app_bouncing_button,      "Bouncing RUN button gives a single event");
    RUN_TEST("TC60", tc_app_spike_ignored,        "10 ms spike on button ignored");
    RUN_TEST("TC61", tc_app_double_stop,          "Physical STOP pressed twice stops washing");
    RUN_TEST("TC62", tc_app_pause_resume,         "Physical PAUSE / RUN buttons");
    RUN_TEST("TC63", tc_app_invalid_coin,         "25c coin -> RLED pin blinks then recovers");
    RUN_TEST("TC64", tc_app_fault_line,           "Fault line debounced, real fault handled");

    return TF_END();
}
