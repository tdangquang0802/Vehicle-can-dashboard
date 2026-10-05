/**
 * @file    dtc_text.h
 * @brief   DTC code -> text (DIAG-03).
 */
#ifndef DTC_TEXT_H
#define DTC_TEXT_H

#include <stdint.h>

/** Format a 2-byte DTC as "P0217" (5 chars + NUL). Top 2 bits: P, C, B, U. */
void        dtc_str(uint16_t code, char buf[6]);
/** Description from the 4-code table (max 20 chars), "Unknown" otherwise. */
const char *dtc_desc(uint16_t code);

#endif /* DTC_TEXT_H */
