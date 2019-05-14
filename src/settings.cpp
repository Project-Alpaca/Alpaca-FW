#include "settings.h"

Settings::Settings() {};

void Settings::reset() {
    this->magic_ver = this->SETTINGS_VER;
    memset(&this->tp_calib, 0xff, sizeof(calib_t));
    this->tp_calib.zeroLevel = 0;
    this->tp_gap_ratio = 0.0f;
    memset(this->button_mapping, 0x00, 16 * sizeof(uint8_t));
    this->ds4_passthrough = false;
    this->perf_ctr = false;
    this->stick_hold = 24;
    this->max_segs = 20;
    this->seg_mult = true;
    this->crc32 = 0;
}

void Settings::save() {
    CRC32 crc;
    auto &settings = static_cast<SettingsData &>(*this);
    auto *settings_wr = reinterpret_cast<uint8_t *>(&settings);

    crc.update(settings_wr, sizeof(settings)-sizeof(settings.crc32));
    this->crc32 = crc.finalize();
    for (size_t i=0; i<sizeof(settings); i++) {
        // only save when there is a difference
        if (EEPROM.read(i) != settings_wr[i]) {
            EEPROM.write(i, settings_wr[i]);
        }
    }
}

void Settings::load() {
    CRC32 crc;
    auto &settings = static_cast<SettingsData &>(*this);
    auto *settings_wr = reinterpret_cast<uint8_t *>(&settings);

    for (size_t i=0; i<sizeof(settings); i++) {
        settings_wr[i] = EEPROM.read(i);
    }

    crc.update(settings_wr, sizeof(settings)-sizeof(settings.crc32));
    if (this->magic_ver != this->SETTINGS_VER || crc.finalize() != this->crc32) {
        // settings does not exist or corrupted
        this->reset();
        return;
    }
    return;
}
