/**
 * @file    interactive_sim.c
 * @brief   Keyboard-driven, real-time simulator of the washing machine.
 *
 * The REAL controller code (wm_fsm.c, wm_debounce.c, wm_app.c) runs here
 * unchanged; only the HAL is replaced by a fake one driven by the keyboard
 * and displayed on the console.
 *
 *   Keys:  1 = 10c   2 = 20c   3 = 50c   4 = invalid coin (25c)
 *          r = RUN   p = PAUSE s = STOP  f = toggle hardware fault
 *          t = +10 s   n = +1 min   m = +10 min   (fast forward)
 *          + / - = simulation speed (x1, x10, x60, x600)
 *          h = help  q = quit
 *
 * Modes
 *   default   : live dashboard, keys act immediately (no Enter needed)
 *   --plain   : step mode, type commands then Enter; every command advances
 *               the simulated time by 200 ms. Automatically selected when
 *               stdin is not a console (e.g. "echo 3r | interactive_sim").
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "wm_app.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#endif

/* ====================================================================== */
/* Platform layer: keyboard, clock, console                               */
/* ====================================================================== */
#ifdef _WIN32
static DWORD g_old_out_mode;
static HANDLE g_hout;

static void con_setup(void)
{
    DWORD mode;
    g_hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(g_hout, &mode)) {
        g_old_out_mode = mode;
        SetConsoleMode(g_hout, mode | 0x0004u /* VIRTUAL_TERMINAL_PROCESSING */);
    }
}
static void con_restore(void)   { SetConsoleMode(g_hout, g_old_out_mode); }
static bool key_available(void) { return _kbhit() != 0; }
static int  key_read(void)
{
    int c = _getch();
    if (c == 0 || c == 0xE0) { (void)_getch(); return -1; }   /* arrow keys etc. */
    return c;
}
static uint64_t now_ms(void)    { return (uint64_t)GetTickCount64(); }
static void sleep_ms(unsigned ms) { Sleep(ms); }
static bool stdin_is_tty(void)  { return _isatty(_fileno(stdin)) != 0; }
#else
static struct termios g_old_tio;
static bool g_tio_saved = false;

static void con_setup(void)
{
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &g_old_tio) == 0) {
        g_tio_saved = true;
        t = g_old_tio;
        t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
        t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
}
static void con_restore(void)
{
    if (g_tio_saved) tcsetattr(STDIN_FILENO, TCSANOW, &g_old_tio);
}
static bool key_available(void)
{
    fd_set fds; struct timeval tv = {0, 0};
    FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
static int key_read(void)
{
    unsigned char c;
    return (read(STDIN_FILENO, &c, 1) == 1) ? (int)c : -1;
}
static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
static void sleep_ms(unsigned ms)
{
    struct timespec ts = { 0, (long)ms * 1000000L };
    nanosleep(&ts, NULL);
}
static bool stdin_is_tty(void) { return isatty(STDIN_FILENO) != 0; }
#endif

/* ====================================================================== */
/* Fake hardware (HAL implementation for the simulator)                   */
/* ====================================================================== */
#define PRESS_TICKS    8u      /* a key press holds the button 80 ms   */
#define RELEASE_TICKS  6u      /* minimum release gap between presses  */
#define COIN_QUEUE     16u

typedef struct {
    uint32_t hold[WM_BTN_COUNT];     /* ticks left with button pressed */
    uint32_t gap[WM_BTN_COUNT];      /* ticks left with button released */
    uint32_t pending[WM_BTN_COUNT];  /* queued key presses */
    uint16_t coins[COIN_QUEUE];
    unsigned coin_head, coin_count;
    bool     fault;
    bool     rled, bled, motor;
} sim_hw_t;

static sim_hw_t hw;

static bool hal_read_button(wm_button_id_t id) { return hw.hold[id] > 0u; }
static uint16_t hal_read_coin(void)
{
    uint16_t c;
    if (hw.coin_count == 0u) return 0u;
    c = hw.coins[hw.coin_head];
    hw.coin_head = (hw.coin_head + 1u) % COIN_QUEUE;
    hw.coin_count--;
    return c;
}
static bool hal_read_fault(void)     { return hw.fault; }
static void hal_write_rled(bool on)  { hw.rled  = on; }
static void hal_write_bled(bool on)  { hw.bled  = on; }
static void hal_write_motor(bool on) { hw.motor = on; }

static const wm_hal_t k_sim_hal = {
    hal_read_button, hal_read_coin, hal_read_fault,
    hal_write_rled, hal_write_bled, hal_write_motor
};

static void hw_press(wm_button_id_t id) { hw.pending[id]++; }
static void hw_coin(uint16_t cents)
{
    if (hw.coin_count < COIN_QUEUE) {
        hw.coins[(hw.coin_head + hw.coin_count) % COIN_QUEUE] = cents;
        hw.coin_count++;
    }
}

/** Advance the simulated button waveforms by one tick. */
static void hw_step(void)
{
    unsigned i;
    for (i = 0; i < (unsigned)WM_BTN_COUNT; i++) {
        if (hw.hold[i] > 0u) {
            if (--hw.hold[i] == 0u) hw.gap[i] = RELEASE_TICKS;
        } else if (hw.gap[i] > 0u) {
            hw.gap[i]--;
        } else if (hw.pending[i] > 0u) {
            hw.pending[i]--;
            hw.hold[i] = PRESS_TICKS;
        }
    }
}

/* ====================================================================== */
/* Simulator state, event log                                             */
/* ====================================================================== */
static wm_app_t app;
static bool     g_plain = false;
static bool     g_quit  = false;
static unsigned g_speed_idx = 0;
static const unsigned k_speeds[] = { 1u, 10u, 60u, 600u };
#define N_SPEEDS (sizeof(k_speeds) / sizeof(k_speeds[0]))

#define LOG_LINES 10
#define LOG_WIDTH 160
static char     g_log[LOG_LINES][LOG_WIDTH];
static unsigned g_log_head = 0, g_log_count = 0;

static void fmt_time(char *buf, size_t n, uint32_t ticks)
{
    uint64_t ms = (uint64_t)ticks * WM_TICK_MS;
    unsigned h  = (unsigned)(ms / 3600000u);
    unsigned mi = (unsigned)((ms / 60000u) % 60u);
    unsigned s  = (unsigned)((ms / 1000u) % 60u);
    unsigned cs = (unsigned)((ms / 10u) % 100u);
    snprintf(buf, n, "%02u:%02u:%02u.%02u", h, mi, s, cs);
}

static void log_msg(const char *fmt, ...)
{
    char    t[32], body[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    fmt_time(t, sizeof(t), app.tick_count);

    if (g_plain) {
        printf("  %s  %s\n", t, body);
        return;
    }
    snprintf(g_log[(g_log_head + g_log_count) % LOG_LINES], LOG_WIDTH, "%s  %s", t, body);
    if (g_log_count < LOG_LINES) g_log_count++;
    else g_log_head = (g_log_head + 1u) % LOG_LINES;
}

/** Listener: called by wm_app for every event delivered to the FSM. */
static void on_event(void *user, wm_event_t ev, wm_result_t res, wm_state_t from, wm_state_t to)
{
    (void)user;
    if (from != to) {
        log_msg("%-12s -> %-13s  %s -> %s", wm_event_name(ev), wm_result_name(res),
                wm_state_name(from), wm_state_name(to));
    } else {
        log_msg("%-12s -> %-13s  (stay %s)", wm_event_name(ev), wm_result_name(res),
                wm_state_name(from));
    }
}

/** One simulated tick: run the controller, then the fake hardware. */
static void sim_step(void)
{
    wm_state_t before       = wm_get_state(&app.fsm);
    bool       armed_before = wm_is_stop_armed(&app.fsm);
    wm_error_t err_before   = wm_get_error(&app.fsm);
    uint32_t   err_ticks_before = app.fsm.error_ticks;
    uint32_t   done_before  = app.fsm.completed_count;

    wm_app_tick(&app);
    hw_step();

    /* report transitions that happen by themselves (timers) */
    if (app.fsm.completed_count != done_before) {
        log_msg("TIMER        -> wash COMPLETED (%lu s)  %s -> %s",
                (unsigned long)(WM_WASH_DURATION_MS / 1000UL),
                wm_state_name(before), wm_state_name(wm_get_state(&app.fsm)));
    } else if (before == WM_STATE_ERROR && err_before == WM_ERR_INVALID_COIN &&
               err_ticks_before <= 1u && wm_get_state(&app.fsm) != WM_STATE_ERROR) {
        log_msg("TIMER        -> coin error cleared       ERROR -> %s",
                wm_state_name(wm_get_state(&app.fsm)));
    }
    if (armed_before && !wm_is_stop_armed(&app.fsm) &&
        (wm_get_state(&app.fsm) == WM_STATE_RUNNING || wm_get_state(&app.fsm) == WM_STATE_PAUSED) &&
        before == wm_get_state(&app.fsm)) {
        log_msg("TIMER        -> STOP window expired (need 2 presses within %u ms)",
                (unsigned)WM_STOP_CONFIRM_WINDOW_MS);
    }
}

static void sim_advance(uint32_t n) { while (n-- && !g_quit) sim_step(); }

/* ====================================================================== */
/* Rendering                                                              */
/* ====================================================================== */
#define C_RESET  "\x1b[0m"
#define C_BOLD   "\x1b[1m"
#define C_DIM    "\x1b[2m"
#define C_CYAN   "\x1b[36m"
#define C_YELLOW "\x1b[33m"
#define C_GREEN  "\x1b[32m"
#define C_RED    "\x1b[31m"
#define C_BLUE   "\x1b[34m"

static const char *state_color(wm_state_t s)
{
    switch (s) {
    case WM_STATE_STANDBY: return C_RED;
    case WM_STATE_READY:   return C_BLUE;
    case WM_STATE_RUNNING: return C_GREEN;
    case WM_STATE_PAUSED:  return C_YELLOW;
    default:               return "\x1b[1;31m";
    }
}

static const char *led_mode_name(wm_led_mode_t m)
{
    return (m == WM_LED_ON) ? "ON   " : (m == WM_LED_BLINK) ? "BLINK" : "OFF  ";
}

static const char *hint(void)
{
    const wm_ctx_t *f = &app.fsm;
    static char buf[96];
    switch (f->state) {
    case WM_STATE_STANDBY:
        snprintf(buf, sizeof(buf), "Insert coins: need %u more cents (keys 1/2/3)",
                 (unsigned)(WM_MIN_CREDIT_CENTS - f->credit_cents));
        return buf;
    case WM_STATE_READY:   return "Enough money! Press [r] RUN to start washing";
    case WM_STATE_RUNNING: return "Washing... [p] pause, [s] twice to force stop";
    case WM_STATE_PAUSED:  return "Paused - timer is STILL counting! [r] resume";
    default:
        return (f->error == WM_ERR_HW_FAULT)
               ? "Hardware fault! Machine locked - press [f] to clear the fault"
               : "Invalid coin returned - wait 3 s or insert a valid coin";
    }
}

static void draw_dashboard(void)
{
    static char  out[8192];
    size_t       n = 0;
    char         t[32];
    const wm_ctx_t *f = &app.fsm;
    wm_state_t   s  = f->state;
    uint32_t     rem = wm_get_remaining_seconds(f);
    unsigned     i, bar_fill = 0;
    static const char spin[4] = { '|', '/', '-', '\\' };

#define EMIT(...) do { int k_ = snprintf(out + n, sizeof(out) - n, __VA_ARGS__); \
                      if (k_ > 0 && (size_t)k_ < sizeof(out) - n) n += (size_t)k_; } while (0)
#define EOL "\x1b[K\n"

    fmt_time(t, sizeof(t), app.tick_count);
    if (f->remaining_ticks > 0u) {
        bar_fill = (unsigned)(30u - (uint64_t)f->remaining_ticks * 30u / WM_WASH_TICKS);
    }

    EMIT("\x1b[H");
    EMIT(C_CYAN " +==============================================================+" EOL);
    EMIT(" |" C_BOLD "        WASHING MACHINE CONTROLLER  -  CO3053 SIMULATOR       " C_RESET C_CYAN "|" EOL);
    EMIT(" +==============================================================+" C_RESET EOL);
    EMIT("   Sim time : %s          Speed : " C_BOLD "x%u" C_RESET EOL, t, k_speeds[g_speed_idx]);
    EMIT(EOL);
    EMIT("   State    : %s" C_BOLD "%-8s" C_RESET, state_color(s), wm_state_name(s));
    if (s == WM_STATE_ERROR) EMIT("  (%s)", wm_error_name(f->error));
    EMIT(EOL);
    EMIT("   RLED     : %s   " C_RESET " %s     BLED : %s   " C_RESET " %s" EOL,
        hw.rled ? "\x1b[41m" : "\x1b[100m", led_mode_name(wm_get_rled_mode(f)),
        hw.bled ? "\x1b[44m" : "\x1b[100m", led_mode_name(wm_get_bled_mode(f)));
    if (hw.motor) {
        EMIT("   Motor    : " C_GREEN "ON " C_RESET "  %c spinning" EOL, spin[(app.tick_count / 10u) % 4u]);
    } else {
        EMIT("   Motor    : OFF" EOL);
    }
    EMIT("   Credit   : " C_BOLD "%u" C_RESET " cents  (need %u)" EOL,
        (unsigned)f->credit_cents, (unsigned)WM_MIN_CREDIT_CENTS);
    if (f->remaining_ticks > 0u) {
        EMIT("   Timer    : " C_BOLD "%02u:%02u" C_RESET " remaining  [", (unsigned)(rem / 60u), (unsigned)(rem % 60u));
        for (i = 0; i < 30u; i++) EMIT("%c", i < bar_fill ? '#' : '.');
        EMIT("]" EOL);
    } else {
        EMIT("   Timer    : --:--" EOL);
    }
    if (f->stop_armed) {
        EMIT("   STOP     : " C_YELLOW "ARMED" C_RESET " - press [s] again within %.1f s to force stop" EOL,
            (double)f->stop_window_ticks * WM_TICK_MS / 1000.0);
    } else {
        EMIT("   STOP     : -" EOL);
    }
    EMIT("   Stats    : completed=%lu  force-stopped=%lu   Fault line: %s" EOL,
        (unsigned long)f->completed_count, (unsigned long)f->force_stop_count,
        hw.fault ? C_RED "ACTIVE" C_RESET : "ok");
    EMIT(EOL);
    EMIT("   " C_BOLD ">> %s" C_RESET EOL, hint());
    EMIT(C_DIM " ----------------------------------------------------------------" C_RESET EOL);
    EMIT("   [1] 10c   [2] 20c   [3] 50c   [4] invalid coin (25c)" EOL);
    EMIT("   [r] RUN   [p] PAUSE [s] STOP  [f] toggle hardware fault" EOL);
    EMIT("   [t] +10s  [n] +1min [m] +10min   [+/-] speed   [q] quit" EOL);
    EMIT(C_DIM " ----------------------------------------------------------------" C_RESET EOL);
    EMIT("   Event log:" EOL);
    for (i = 0; i < LOG_LINES; i++) {
        if (i < g_log_count) EMIT("    %s" EOL, g_log[(g_log_head + i) % LOG_LINES]);
        else EMIT(EOL);
    }
    EMIT("\x1b[J");
#undef EMIT
#undef EOL
    fwrite(out, 1, n, stdout);
    fflush(stdout);
}

static void print_status_plain(void)
{
    const wm_ctx_t *f = &app.fsm;
    char t[32];
    uint32_t rem = wm_get_remaining_seconds(f);
    fmt_time(t, sizeof(t), app.tick_count);
    printf("  ---- [%s] state=%-7s", t, wm_state_name(f->state));
    if (f->state == WM_STATE_ERROR) printf("(%s)", wm_error_name(f->error));
    printf(" RLED=%s BLED=%s motor=%s credit=%uc",
           led_mode_name(wm_get_rled_mode(f)), led_mode_name(wm_get_bled_mode(f)),
           hw.motor ? "ON" : "OFF", (unsigned)f->credit_cents);
    if (f->remaining_ticks > 0u) printf(" timer=%02u:%02u", (unsigned)(rem / 60u), (unsigned)(rem % 60u));
    if (f->stop_armed) printf(" STOP-ARMED");
    printf("\n");
}

static void print_help(void)
{
    printf("\n  Washing machine simulator - commands\n"
           "    1 = insert 10c    2 = insert 20c    3 = insert 50c    4 = invalid coin (25c)\n"
           "    r = RUN           p = PAUSE         s = STOP          f = toggle hardware fault\n"
           "    t = +10 seconds   n = +1 minute     m = +10 minutes   i = show status\n"
           "    h = help          q = quit\n");
    if (g_plain) {
        printf("  Step mode: type one or more commands then Enter (e.g. \"33r\"),\n"
               "  each command advances the simulated time by 200 ms.\n\n");
    }
}

/* ====================================================================== */
/* Command handling                                                       */
/* ====================================================================== */
static void handle_key(int c)
{
    switch (c) {
    case '1': hw_coin(10); break;
    case '2': hw_coin(20); break;
    case '3': hw_coin(50); break;
    case '4': hw_coin(25); break;
    case 'r': case 'R': hw_press(WM_BTN_RUN);   break;
    case 'p': case 'P': hw_press(WM_BTN_PAUSE); break;
    case 's': case 'S': hw_press(WM_BTN_STOP);  break;
    case 'f': case 'F':
        hw.fault = !hw.fault;
        log_msg("[fault line %s]", hw.fault ? "ACTIVE (e.g. door opened)" : "released");
        break;
    case 't': case 'T': log_msg("[fast forward +10 s]");  sim_advance(WM_MS_TO_TICKS(10000u));  break;
    case 'n': case 'N': log_msg("[fast forward +1 min]"); sim_advance(WM_MS_TO_TICKS(60000u));  break;
    case 'm': case 'M': log_msg("[fast forward +10 min]"); sim_advance(WM_MS_TO_TICKS(600000u)); break;
    case '+': case '=':
        if (g_speed_idx + 1u < N_SPEEDS) g_speed_idx++;
        log_msg("[speed x%u]", k_speeds[g_speed_idx]);
        break;
    case '-': case '_':
        if (g_speed_idx > 0u) g_speed_idx--;
        log_msg("[speed x%u]", k_speeds[g_speed_idx]);
        break;
    case 'i': case 'I': if (g_plain) print_status_plain(); break;
    case 'h': case 'H': case '?': if (g_plain) print_help(); break;
    case 'q': case 'Q': case 27: g_quit = true; break;
    default: break;
    }
}

/* ---------------------------------------------------------------------- */
static void run_plain(void)
{
    int c;
    print_help();
    print_status_plain();
    printf("> ");
    fflush(stdout);
    while (!g_quit && (c = getchar()) != EOF) {
        if (c == '\n' || c == '\r') {
            print_status_plain();
            printf("> ");
            fflush(stdout);
            continue;
        }
        if (c == ' ' || c == '\t') continue;
        handle_key(c);
        if (strchr("1234rRpPsSfF", c) != NULL) {
            sim_advance(WM_MS_TO_TICKS(200u));      /* let debouncer see the press */
        }
    }
    printf("\n");
}

static void run_live(void)
{
    uint64_t last = now_ms(), last_draw = 0;
    uint64_t acc_ms = 0;

    con_setup();
    printf("\x1b[2J\x1b[?25l");                     /* clear screen, hide cursor */
    log_msg("Simulator started - machine on STANDBY");

    while (!g_quit) {
        uint64_t t;
        uint32_t n;

        while (key_available()) {
            int c = key_read();
            if (c >= 0) handle_key(c);
        }

        t = now_ms();
        acc_ms += (t - last) * k_speeds[g_speed_idx];
        last = t;
        n = (uint32_t)(acc_ms / WM_TICK_MS);
        acc_ms -= (uint64_t)n * WM_TICK_MS;
        if (n > 200000u) n = 200000u;
        sim_advance(n);

        if (t - last_draw >= 40u) {                 /* ~25 frames per second */
            draw_dashboard();
            last_draw = t;
        }
        sleep_ms(5);
    }
    draw_dashboard();
    printf("\x1b[?25h\nBye!\n");                    /* show cursor again */
    con_restore();
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--plain") == 0) g_plain = true;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            printf("  Options: --plain  step mode (type commands + Enter)\n");
            return 0;
        }
    }
    if (!stdin_is_tty()) g_plain = true;            /* piped input -> step mode */

#ifdef _WIN32
    con_setup();                                    /* enable ANSI colours */
#endif
    memset(&hw, 0, sizeof(hw));
    if (!wm_app_init(&app, &k_sim_hal)) {
        fprintf(stderr, "HAL init failed\n");
        return 1;
    }
    wm_app_set_listener(&app, on_event, NULL);

    if (g_plain) run_plain();
    else         run_live();
    return 0;
}
