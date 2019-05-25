#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <CRC32.h>
#include <SoftPotMagic.h>

struct SettingsData {
    uint32_t magic_ver;
    calib_t tp_calib;
    uint16_t tp_gap_ratio;
    uint8_t default_tp_mode;
    bool ds4_passthrough;
    bool perf_ctr;
    uint8_t stick_hold;
    uint8_t max_segs;
    bool seg_mult;
    uint32_t crc32;
} __attribute__((packed));

class Settings : public SettingsData {
public:
    Settings();
    void load();
    void save();
    void reset();
private:
    const uint32_t SETTINGS_VER = 0x0a00af05;
};
