#pragma once

#include <EEPROM.h>
#include <SoftPotMagic.h>

#define SETTINGS_VER 0x0700af05

typedef struct {
    uint32_t magic_ver;
    calib_t tp_calib;
    float tp_gap_ratio;
    uint8_t button_mapping[16];
    uint8_t default_tp_mode;
    bool ds4_passthrough;
    bool perf_ctr;
    uint8_t stick_hold;
    uint32_t crc32;
} __attribute__((packed)) settings_t;

extern void settings_init(settings_t *settings);
extern void settings_save(settings_t *settings);
extern void settings_load(settings_t *settings);

extern settings_t controller_settings;
