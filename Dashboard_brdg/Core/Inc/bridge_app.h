/**
 * @file    bridge_app.h
 * @brief   Bridge entry points, called from CubeMX's main.c.
 */
#ifndef BRIDGE_APP_H
#define BRIDGE_APP_H

/** Call once in main() inside "USER CODE BEGIN 2". */
void bridge_setup(void);
/** Call inside the while(1) loop, "USER CODE BEGIN 3". Never blocks. */
void bridge_loop(void);

#endif /* BRIDGE_APP_H */
