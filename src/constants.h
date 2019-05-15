#pragma once

#include <Arduino.h>
#include <SPI.h>

enum class TPMode : uint8_t {
    TP = 0, DPAD, LR, TP_C, ATRF, _TOTAL_MODES,
};

extern const char BUTTON_NAMES[];
extern const char TP_MODES[];

//#define TP_MODE_CUSTOM1 0x10
//#define TP_MODE_CUSTOM2 0x11
//#define TP_MODE_CUSTOM3 0x12

// TODO remove those
#define BTN2DS4(map) (map - 1 - 4)
#define ISBTN(map) (map >= DS4.KEY_SQR+4 && map <= DS4.KEY_TP+4)
#define ISDPAD(map) (map >= BTN_U && map <= BTN_R)

#define BTN_NUL 0
#define BTN_U 1
#define BTN_L 2
#define BTN_D 3
#define BTN_R 4
#define BTN_SQR 5
#define BTN_XRO 6
#define BTN_CIR 7
#define BTN_TRI 8
#define BTN_L1 9
#define BTN_R1 10
#define BTN_L2 11
#define BTN_R2 12
#define BTN_SHR 13
#define BTN_OPT 14
#define BTN_L3 15
#define BTN_R3 16
#define BTN_PS 17
#define BTN_TP 18

/*
#define BTN_PRECONF { \
    BTN_CIR, BTN_XRO, BTN_SQR, BTN_TRI, BTN_NUL, BTN_NUL, BTN_NUL, BTN_NUL, \
    BTN_SHR, BTN_PS , BTN_OPT, BTN_NUL, BTN_NUL, BTN_NUL, BTN_NUL, BTN_NUL  \
}
*/

extern const SPISettings SPI596;
extern const SPISettings SPI4021;
extern const uint16_t ATRF_SEG_WIDTH;

#include "pin_config.h"
