#pragma once

// IO pin definitions
#include <Arduino.h>
#include <Encoder.h>
#include <LiquidCrystal.h>
#include <ResponsiveAnalogRead.h>

extern Encoder QEI;
extern ResponsiveAnalogRead RAL;
extern ResponsiveAnalogRead RAR;
extern LiquidCrystal LCD;

extern int respAnalogRead(uint8_t pin);
