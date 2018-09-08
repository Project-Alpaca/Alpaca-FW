#pragma once

#include <Arduino.h>
#include <SPI.h>

#define TP_NB_MODES 5
#define TP_MODE_TP 0x00
#define TP_MODE_DPAD 0x01
#define TP_MODE_LR 0x02
#define TP_MODE_TP_C 0x03
#define TP_MODE_TP_A 0x04

//#define TP_MODE_CUSTOM1 0x10
//#define TP_MODE_CUSTOM2 0x11
//#define TP_MODE_CUSTOM3 0x12

#define TP_MODE_TP_N "TP"
#define TP_MODE_DPAD_N "DPAD"
#define TP_MODE_LR_N "LR"
#define TP_MODE_TP_C_N "TP+C"
#define TP_MODE_TP_A_N "TP+A"

#define BTN2DS4(map) (map - 1)
#define ISBTN(map) (map >= BTN_SQR && map <= BTN_TP)
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

const SPISettings SPI596 = SPISettings(2000000, MSBFIRST, SPI_MODE0);
const SPISettings SPI4021 = SPISettings(2000000, MSBFIRST, SPI_MODE0);

#include "pin_config.h"
