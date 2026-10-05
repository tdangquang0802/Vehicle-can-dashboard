/**
 * @file    ecu_app.h
 * @brief   ECU application entry points, to be called from CubeMX's main.c.
 */
#ifndef ECU_APP_H
#define ECU_APP_H

/** Call once in main() inside "USER CODE BEGIN 2". */
void ecu_setup(void);
/** Call inside the while(1) loop, "USER CODE BEGIN 3". Never blocks. */
void ecu_loop(void);

#endif /* ECU_APP_H */
