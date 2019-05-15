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
const char BUTTON_NAMES[] = "NUL|U|L|D|R|SQR|XRO|CIR|TRI|L1|R1|L2|R2|SHR|OPT|L3|R3|PS|TP";
// Make sure the names are exactly 4 bytes long, otherwise mode display will NOT work
// Might send a PR to MD_Menu for support of NUL-splitted lists.
const char TP_MODES[] = "TP  |DPAD|LR  |TP+C|ATRF";
const uint16_t ATRF_SEG_WIDTH = 96;

const SPISettings SPI596 = SPISettings(2000000, MSBFIRST, SPI_MODE0);
const SPISettings SPI4021 = SPISettings(2000000, MSBFIRST, SPI_MODE0);

int respAnalogRead(uint8_t pin) {
    switch (pin) {
        case SP_L:
            return RAL.getValue();
            break;
        case SP_R:
            return RAR.getValue();
            break;
        default:
            return analogRead(pin);
    }
}
