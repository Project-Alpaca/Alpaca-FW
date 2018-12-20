#include <Arduino.h>
#include <EEPROM.h>
#include <InternalCRC32.h>
#include <SoftPotMagic.h>
#include "settings.h"

void settings_init(settings_t *settings) {
    settings->magic_ver = SETTINGS_VER;
    memset(&settings->tp_calib, 0xff, sizeof(calib_t));
    settings->tp_calib.zeroLevel = 0;
    settings->tp_gap_ratio = 0.0f;
    memset(&settings->button_mapping, 0x00, 16 * sizeof(uint8_t));
    settings->ds4_passthrough = false;
    settings->perf_ctr = false;
    settings->stick_hold = 24;
    settings->max_segs = 20;
    settings->seg_mult = true;
    settings->crc32 = 0;
}

void settings_save(settings_t *settings) {
    InternalCRC32 crc;
    uint8_t *settings_wr = (uint8_t *) settings;

    crc.update(settings_wr, sizeof(settings_t)-sizeof(uint32_t));
    settings->crc32 = crc.finalize();
    for (size_t i=0; i<sizeof(settings_t); i++) {
        // only save when there is a difference
        if (EEPROM.read(i) != settings_wr[i]) {
            EEPROM.write(i, settings_wr[i]);
        }
    }
}

void settings_load(settings_t *settings) {
    InternalCRC32 crc;
    uint8_t *settings_wr = (uint8_t *) settings;

    for (size_t i=0; i<sizeof(settings_t); i++) {
        settings_wr[i] = EEPROM.read(i);
    }

    crc.update(settings_wr, sizeof(settings_t)-sizeof(uint32_t));
    if (settings->magic_ver != SETTINGS_VER || crc.finalize() != settings->crc32) {
        // settings does not exist or corrupted
        settings_init(settings);
        return;
    }
    return;
}

settings_t controller_settings;
