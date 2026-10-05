/**
 * @file    obd_srv.h
 * @brief   OBD-II server on the ECU: services 01, 03, 07, 04 (ECU-06..10).
 *
 * NOTE: the design calls this function obd_poll(); it is named obdsrv_poll()
 * here so the ECU server and the Diag client can be linked into the same host
 * test binary without a symbol clash.
 */
#ifndef OBD_SRV_H
#define OBD_SRV_H

/** Call every main-loop iteration: sends a pending reply, handles one request. */
void obdsrv_poll(void);

#endif /* OBD_SRV_H */
