// Objects (mostly for I/Os) that are shared across normal mode and service
// mode

#include "common_objs.h"
#include "constants.h"

Encoder QEI(QEI_A, QEI_B);
ResponsiveAnalogRead RAL(SP_L, true);
ResponsiveAnalogRead RAR(SP_R, true);
LiquidCrystalNew LCD(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7, 0xff);
Settings cfg;

// Common constants

// Make sure the names are exactly 4 bytes long, otherwise mode display will NOT work
// Might send a PR to MD_Menu for support of NUL-separated lists.
const char TP_MODES[] = "TP  |DPAD|LR  |TP+C|ATRF|TTRF|AR  ";
const uint16_t ATRF_SEG_WIDTH = 96;

constexpr Button BUTTON_MAPPING[16] = {
    Button::CIR, Button::XRO, Button::SQR, Button::TRI, Button::NUL, Button::NUL, Button::NUL, Button::NUL,
    Button::SHR, Button::PS, Button::OPT, Button::NUL, Button::NUL, Button::NUL, Button::NUL, Button::NUL,
};

constexpr size_t BUTTON_MAPPING_LEN = sizeof(BUTTON_MAPPING) / sizeof(Button);

const SPISettings SPI596 = SPISettings(2000000, MSBFIRST, SPI_MODE0);
const SPISettings SPI4021 = SPISettings(2000000, MSBFIRST, SPI_MODE0);

int respAnalogRead(uint8_t pin) {
    switch (pin) {
        case SP_L:
            RAL.update();
            return RAL.getValue();
            break;
        case SP_R:
            RAR.update();
            return RAR.getValue();
            break;
        default:
            return analogRead(pin);
    }
}
