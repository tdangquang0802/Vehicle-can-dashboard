/**
 * @file    buttons.h
 * @brief   3 push buttons (Live / Read / Clear), active low, 20 ms debounce.
 */
#ifndef BUTTONS_H
#define BUTTONS_H

typedef enum { BTN_NONE, BTN_LIVE, BTN_READ, BTN_CLEAR } btn_t;

/**
 * Call every 10 ms. Returns one press event (falling edge) per call, after the
 * level was identical on two consecutive samples (20 ms). If several buttons
 * were pressed together, the remaining events are returned on the next calls.
 */
btn_t buttons_poll_10ms(void);

#endif /* BUTTONS_H */
