/**
 * @file    wm_hal.h
 * @brief   Hardware Abstraction Layer interface.
 *
 * The application layer (wm_app.c) talks to the hardware only through this
 * table of function pointers. To port the controller to a new board, write
 * one small file that fills a wm_hal_t with the board-specific functions
 * (GPIO read/write, coin acceptor driver...). The PC test bench and the
 * interactive simulator provide their own fake implementations.
 */
#ifndef WM_HAL_H
#define WM_HAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Identifiers of the three push buttons. */
typedef enum {
    WM_BTN_STOP = 0,
    WM_BTN_RUN,
    WM_BTN_PAUSE,
    WM_BTN_COUNT
} wm_button_id_t;

typedef struct {
    /** Raw (not debounced) level of a button: true = pressed. */
    bool     (*read_button)(wm_button_id_t id);

    /**
     * Coin acceptor. Returns the value (in cents) of a coin that has just
     * been detected, or 0 when no coin arrived since the last call.
     * Any value other than 10/20/50 is treated as an invalid coin.
     */
    uint16_t (*read_coin)(void);

    /** Raw level of the fault line (door open, no water...): true = fault. */
    bool     (*read_fault)(void);

    void     (*write_rled)(bool on);
    void     (*write_bled)(bool on);
    void     (*write_motor)(bool on);
} wm_hal_t;

#ifdef __cplusplus
}
#endif

#endif /* WM_HAL_H */
