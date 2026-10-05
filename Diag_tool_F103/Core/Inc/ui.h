/**
 * @file    ui.h
 * @brief   Diag tool user interface: 4 screens, 3 buttons (DUI-02..06, DUI-08).
 */
#ifndef UI_H
#define UI_H

void ui_init(void);
/** Call every 10 ms: buttons, OBD client control, redraw of changed cells only. */
void ui_run_10ms(void);

#endif /* UI_H */
