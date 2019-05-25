#pragma once

// IO pin definitions
#include <Arduino.h>
#include <Encoder.h>
#include <LiquidCrystalNew.h>
#include <ResponsiveAnalogRead.h>
#include "settings.h"
#include "constants.h"

extern Encoder QEI;
extern ResponsiveAnalogRead RAL;
extern ResponsiveAnalogRead RAR;
extern LiquidCrystalNew LCD;
extern Settings cfg;
extern const Button BUTTON_MAPPING[];
extern const size_t BUTTON_MAPPING_LEN;

extern int respAnalogRead(uint8_t pin);
