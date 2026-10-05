/**
 * @file    dtc_text.c
 */
#include "dtc_text.h"
#include "protocol.h"

void dtc_str(uint16_t code, char buf[6])
{
    static const char hex[] = "0123456789ABCDEF";

    buf[0] = "PCBU"[(code >> 14) & 0x3u];
    buf[1] = (char)('0' + ((code >> 12) & 0x3u));      /* first digit is 0..3 */
    buf[2] = hex[(code >> 8) & 0xFu];
    buf[3] = hex[(code >> 4) & 0xFu];
    buf[4] = hex[code & 0xFu];
    buf[5] = '\0';
}

const char *dtc_desc(uint16_t code)
{
    switch (code) {
    case DTC_CODE_P0217: return "Coolant overtemp";
    case DTC_CODE_P0118: return "ECT sensor circuit";
    case DTC_CODE_P0562: return "Battery voltage low";
    case DTC_CODE_C0035: return "Wheel speed sensor";
    default:             return "Unknown";
    }
}
