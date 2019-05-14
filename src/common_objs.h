#pragma once

// IO pin definitions
#include <Arduino.h>
#include <Encoder.h>
#include <LiquidCrystalNew.h>
#include <ResponsiveAnalogRead.h>
#include "settings.h"

extern Encoder QEI;
extern ResponsiveAnalogRead RAL;
extern ResponsiveAnalogRead RAR;
extern LiquidCrystalNew LCD;
extern Settings cfg;

extern int respAnalogRead(uint8_t pin);
