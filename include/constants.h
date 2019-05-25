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

enum class Button {
    NUL = 0, U, L, D, R, SQR, XRO, CIR, TRI, L1, R1, L2, R2, SHR, OPT, L3, R3, PS, TP,
};

extern const SPISettings SPI596;
extern const SPISettings SPI4021;
extern const uint16_t ATRF_SEG_WIDTH;

#include "pin_config.h"
