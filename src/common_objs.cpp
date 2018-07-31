// Objects (mostly for I/Os) that are shared across normal mode and service
// mode

#include <Arduino.h>
#include <Encoder.h>
#include <LiquidCrystal.h>
#include <ResponsiveAnalogRead.h>
#include "common_objs.h"
#include "constants.h"

Encoder QEI(QEI_A, QEI_B);
ResponsiveAnalogRead RAL(SP_L, true);
ResponsiveAnalogRead RAR(SP_R, true);
LiquidCrystal LCD(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

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
