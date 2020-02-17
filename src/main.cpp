#include <Arduino.h>

#include <EventResponder.h>
#include <RDS4-DS4.hpp>

#include <usbhub.h>
#include <PS4USB.h>
#include <SoftPotMagic.h>

#include <SPI.h>
#include <Encoder.h>
#include <LiquidCrystalNew.h>
#include <ResponsiveAnalogRead.h>

#include "service_menu.h"
#include "common_objs.h"
#include "constants.h"

namespace rds4api = rds4::api;
namespace ds4 = rds4::ds4;

// Debug info
#if defined(RDS4_DEBUG)
    #define DEBUG_CONSOLE_INIT() Serial1.begin(115200)
    #define DEBUG_CONSOLE_PRINT(args) Serial1.print(args)
    #define DEBUG_CONSOLE_PRINTLN(args) Serial1.println(args)
#else
    #define DEBUG_CONSOLE_INIT() while (0) {}
    #define DEBUG_CONSOLE_PRINT(args) while (0) {}
    #define DEBUG_CONSOLE_PRINTLN(args) while (0) {}
#endif

// Authenticator DS4 controller over USBH. Also used for DS4 passthrough.
USB USBH;
USBHub Hub1(&USBH);
ds4::PS4USB2 RealDS4(&USBH);

// PS4 controller framework
ds4::AuthenticatorUSBH DS4A(&RealDS4);
ds4::TransportTeensy DS4T(&DS4A);
ds4::ControllerSOCD<> DS4(&DS4T);

// Various events (tasks)
EventResponder ScanEvent;
EventResponder USBHTaskEvent;
EventResponder LowSpeedScanEvent;
EventResponder LCDPerfEvent;
EventResponder DS4TUpdateEvent;
MillisTimer USBHTaskTimer;
MillisTimer LowSpeedScanTimer;
MillisTimer LCDPerfTimer;

// Misc. persistent states
uint8_t lamps;
uint16_t buttons;
TPMode tp_mode;

// Performance counters (scans-per-second, reports-per-second and elapsed time during scanning)
static uint16_t sps = 0;
static uint16_t fps = 0;
static uint32_t etds = 0;

static inline void scan_buttons() {
    uint8_t lamps_new;
    uint8_t dpad = 0;

    digitalWrite(BTN_CSB, LOW);
    SPI.beginTransaction(SPI4021);
    buttons = SPI.transfer(0x00);
    buttons |= SPI.transfer(0x00) << 8;
    digitalWrite(BTN_CSB, HIGH);
    SPI.endTransaction();

    // saves one SPI transaction if lamp is not updated
    // preserve state for lamp 4-7 for now. Maybe make this configurable in the future?
    lamps_new = (buttons & 0x0f) | (lamps & 0xf0);
    if (lamps_new != lamps) {
        digitalWrite(BTN_CSL, LOW);
        SPI.beginTransaction(SPI596);
        SPI.transfer(lamps_new);
        digitalWrite(BTN_CSL, HIGH);
        SPI.endTransaction();
        lamps = lamps_new;
    }

    for (size_t i=0; i<BUTTON_MAPPING_LEN; i++) {
        bool pressed = !(buttons & (1 << i));
        switch (BUTTON_MAPPING[i]) {
        case Button::NUL:
            break;
        case Button::U:
            dpad |= pressed;
            break;
        case Button::L:
            dpad |= pressed << 1;
            break;
        case Button::D:
            dpad |= pressed << 2;
            break;
        case Button::R:
            dpad |= pressed << 3;
            break;
        case Button::SQR:
            DS4.setKeyUniversal(rds4api::Key::Y, pressed);
            break;
        case Button::XRO:
            DS4.setKeyUniversal(rds4api::Key::B, pressed);
            break;
        case Button::CIR:
            DS4.setKeyUniversal(rds4api::Key::A, pressed);
            break;
        case Button::TRI:
            DS4.setKeyUniversal(rds4api::Key::X, pressed);
            break;
        case Button::L1:
            DS4.setKeyUniversal(rds4api::Key::LButton, pressed);
            break;
        case Button::R1:
            DS4.setKeyUniversal(rds4api::Key::RButton, pressed);
            break;
        case Button::L2:
            DS4.setKeyUniversal(rds4api::Key::LTrigger, pressed);
            break;
        case Button::R2:
            DS4.setKeyUniversal(rds4api::Key::RTrigger, pressed);
            break;
        case Button::SHR:
            DS4.setKeyUniversal(rds4api::Key::Select, pressed);
            break;
        case Button::OPT:
            DS4.setKeyUniversal(rds4api::Key::Start, pressed);
            break;
        case Button::L3:
            DS4.setKeyUniversal(rds4api::Key::LStick, pressed);
            break;
        case Button::R3:
            DS4.setKeyUniversal(rds4api::Key::RStick, pressed);
            break;
        case Button::PS:
            DS4.setKeyUniversal(rds4api::Key::Home, pressed);
            break;
        case Button::TP:
            DS4.setKey(DS4.KEY_TP, pressed);
            break;
        }
        DS4.setDpadUniversalSOCD(dpad & 0b1, dpad & 0b1000, dpad & 0b100, dpad & 0b10);
    }
}

void handle_touchpad_direct_mapping(uint8_t pos1, uint8_t pos2, bool click=false) {
    DS4.clearTouchEvents();
    if (pos1 != POS_FLOAT) {
        if (click) {
            // Touchpad keys does not have universal keycode.
            DS4.pressKey(DS4.KEY_TP);
        }
        DS4.setTouchEvent(0, true, map(pos1, POS_MIN, POS_MAX, 0, 1919), 471);
        if (pos2 != POS_FLOAT) {
            DS4.setTouchEvent(1, true, map(pos2, POS_MIN, POS_MAX, 0, 1919), 471);
        } else {
            DS4.setTouchEvent(1, false);
        }
        DS4.finalizeTouchEvent();
    } else {
        if (click) {
            DS4.releaseKey(DS4.KEY_TP);
        }
    }
}

void handle_touchpad_atrf(uint8_t pos1, uint8_t pos2, bool ttrf=false) {
    static uint8_t pos1_prev = POS_FLOAT, pos2_prev = POS_FLOAT;
    static uint8_t stick_hold_frames = 0;
    static bool released = true;
    static const int16_t tp_max = ATRF_SEG_WIDTH * cfg.max_segs - 1;

    bool is_chain_slide = (DS4.getRumbleIntensityRight() > 0);
    bool dir_changed = false;

    if (is_chain_slide) {
        // Start fresh
        DS4.clearTouchEvents();
        // immediately clear stick states
        stick_hold_frames = 0;
        if (pos1 != POS_FLOAT) {
            // Map both points to pos1
            // TODO what if 2 points are moving towards different directions?
            DS4.setTouchEvent(0, true, map(pos1, POS_MIN, POS_MAX, 0, tp_max), 314);
            if (cfg.seg_mult) {
                DS4.setTouchEvent(1, true, map(pos1, POS_MIN, POS_MAX, 0, tp_max), 628);
            }
            DS4.finalizeTouchEvent();
        }
        // otherwise, do nothing since the touch is already released
    // Handle TTRF (direct touchpad passthrough on normal slides)
    } else if (ttrf) {
        handle_touchpad_direct_mapping(pos1, pos2);
    } else {
        // starts from a clean touchpad state
        DS4.clearTouchEvents();
        // if slider is being touched
        // TODO maybe make it less sensitive to turns
        if (pos1 != POS_FLOAT) {
            // and we can determine the direction
            if (pos1_prev != POS_FLOAT) {
                // left
                if (pos1 < pos1_prev) {
                    DS4.setStick(rds4api::Stick::L, 0, 127);
                    dir_changed = true;
                // right
                } else if (pos1 > pos1_prev) {
                    DS4.setStick(rds4api::Stick::L, 255, 127);
                    dir_changed = true;
                }
            }
            // same for the second touch point
            if (pos2 != POS_FLOAT && pos2_prev != POS_FLOAT) {
                // left
                if (pos2 < pos2_prev) {
                    DS4.setStick(rds4api::Stick::R, 0, 127);
                    dir_changed = true;
                // right
                } else if (pos2 > pos2_prev) {
                    DS4.setStick(rds4api::Stick::R, 255, 127);
                    dir_changed = true;
                }
            }
        }
        // check if direction updates. If so, reset frame counter
        if (dir_changed) {
            stick_hold_frames = cfg.stick_hold;
            released = false;
        }
    }
    // if waiting for release, then just release
    if (!released && stick_hold_frames == 0) {
        DS4.setStick(rds4api::Stick::L, 127, 127);
        DS4.setStick(rds4api::Stick::R, 127, 127);
        released = true;
    }
    if (stick_hold_frames > 0) stick_hold_frames--;
    pos1_prev = pos1;
    pos2_prev = pos2;
}

void handle_touchpad_ar(uint8_t pos1, uint8_t pos2) {
    // https://twitter.com/pickingharres/status/1228902094737817600
    uint32_t bf_sticks = 0x80808080ul;
    uint32_t bf_raw = 0ul;
    if (pos1 != POS_FLOAT) {
        bf_raw |= 1 << (31 - (pos1 >> 3));
        if (pos2 != POS_FLOAT) {
            bf_raw |= 1 << (31 - (pos2 >> 3));
        }
        bf_sticks ^= bf_raw;
    }
    DS4.setStick(rds4api::Stick::L, bf_sticks & 0xffu, (bf_sticks >> 8) & 0xffu);
    DS4.setStick(rds4api::Stick::R, (bf_sticks >> 16) & 0xffu, (bf_sticks >> 24) & 0xffu);
}

static inline void scan_touchpad(void) {
    uint8_t pos1 = POS_FLOAT, pos2 = POS_FLOAT;
    SoftPotMagic.update();
    pos1 = SoftPotMagic.pos1();
    pos2 = SoftPotMagic.pos2();

    switch (tp_mode) {
        case TPMode::AR:
            handle_touchpad_ar(pos1, pos2);
            break;
        case TPMode::ATRF:
            handle_touchpad_atrf(pos1, pos2);
            break;
        case TPMode::TTRF:
            handle_touchpad_atrf(pos1, pos2, true);
            break;
        case TPMode::TP_C:
        case TPMode::TP:
            handle_touchpad_direct_mapping(pos1, pos2, tp_mode == TPMode::TP_C);
            break;
        // D-Pad layout is ULRD, LR layout is L1L2R2R1
        case TPMode::LR: // fall through
        case TPMode::DPAD: {
            DS4.clearTouchEvents();
            uint8_t tmpstate = 0;
            if (pos1 != POS_FLOAT) {
                pos1 = map(pos1, POS_MIN, POS_MAX, 0, 3);
            } else {
                pos1 = 4;
            }
            if (pos2 != POS_FLOAT) {
                pos2 = map(pos2, POS_MIN, POS_MAX, 0, 3);
            } else {
                pos2 = 4;
            }
            // 0 is the leftmost, 3 is the rightmost
            if (pos1 != 4) tmpstate |= 1 << pos1;
            if (pos2 != 4) tmpstate |= 1 << pos2;
            if (tp_mode == TPMode::LR) {
                DS4.setKeyUniversal(rds4api::Key::LButton, tmpstate & (1 << 0));
                DS4.setKeyUniversal(rds4api::Key::LTrigger, tmpstate & (1 << 1));
                DS4.setKeyUniversal(rds4api::Key::RTrigger, tmpstate & (1 << 2));
                DS4.setKeyUniversal(rds4api::Key::RButton, tmpstate & (1 << 3));
            } else {
                DS4.setDpadUniversalSOCD(tmpstate & (1 << 0), tmpstate & (1 << 2), tmpstate & (1 << 3), tmpstate & (1 << 1));
            }
            break;
        }
        default:
            break;
    }
}

void redraw_tp_mode(void) {
    // For variable-length strings we should clear the area first (currently not needed)
    //LCD.setCursor(0, 1);
    //LCD.print("    ");
    LCD.setCursor(0, 1);
    // should be sanitized before
    LCD.write(&TP_MODES[static_cast<uint8_t>(tp_mode) * 5], 4);
}

void handle_tp_mode_switch(void) {
    long qei_step = QEI.read() / 4;
    auto tp_mode_index = static_cast<uint8_t>(tp_mode);
    if (qei_step != 0) {
        if (qei_step < 0 && tp_mode_index < -qei_step) {
            tp_mode_index += (static_cast<uint8_t>(TPMode::_TOTAL_MODES) + qei_step);
        } else {
            tp_mode_index += qei_step;
        }
        // Ensures the TP mode always lands in range and be valid.
        tp_mode_index %= static_cast<uint8_t>(TPMode::_TOTAL_MODES);
        tp_mode = static_cast<TPMode>(tp_mode_index);
        redraw_tp_mode();
        // Set to (current position - consumed steps) to prevent step loss on low scanning rate
        QEI.write(QEI.read() - qei_step * 4);
    }
}

void handle_ds4_pass(void) {
    if (!RealDS4.connected()) return;

    // dpad
    DS4.setDpadUniversalSOCD(RealDS4.getButtonPress(UP), \
                             RealDS4.getButtonPress(RIGHT), \
                             RealDS4.getButtonPress(DOWN), \
                             RealDS4.getButtonPress(LEFT));

    // buttons TODO the rest of them
    DS4.setKeyUniversal(rds4api::Key::LButton, RealDS4.getButtonPress(L1));
    DS4.setKeyUniversal(rds4api::Key::RButton, RealDS4.getButtonPress(R1));

    // analog TODO L2R2
    DS4.setStick(rds4api::Stick::L, RealDS4.getAnalogHat(LeftHatX), RealDS4.getAnalogHat(LeftHatY));
    DS4.setStick(rds4api::Stick::R, RealDS4.getAnalogHat(RightHatX), RealDS4.getAnalogHat(RightHatY));

}

void setup() {
    DEBUG_CONSOLE_INIT();
    DEBUG_CONSOLE_PRINTLN("I: Hello");

    pinMode(QEI_SW, INPUT_PULLUP);
    if (digitalRead(QEI_SW) == LOW) {
        // Jump to service menu
        service_menu_main();
        while (1);
    }

    // Read settings from EEPROM
    cfg.load();

    if (cfg.tp_calib.leftMin == -1 || cfg.tp_calib.rightMin == -1) {
        service_menu_main();
        while (1);
    }

    pinMode(BTN_CSL, OUTPUT);
    pinMode(BTN_CSB, OUTPUT);

    digitalWrite(BTN_CSL, HIGH);
    digitalWrite(BTN_CSB, HIGH);

    SPI.begin();

    DEBUG_CONSOLE_PRINTLN("I: init usbh");
    if (USBH.Init() == -1) {
        DEBUG_CONSOLE_PRINTLN("F: timeout waiting for usbh");
        while (1);
    }

    DEBUG_CONSOLE_PRINTLN("I: init ds4d");
    DS4.begin();

    digitalWrite(BTN_CSL, LOW);
    SPI.beginTransaction(SPI596);
    SPI.transfer(0xcf);
    digitalWrite(BTN_CSL, HIGH);
    SPI.endTransaction();
    lamps = 0xcf;
    buttons = 0xffff;

    // Enable power
    USBH.gpioWr(0x01);

    SoftPotMagic.begin(SP_L, SP_R, respAnalogRead);
    SoftPotMagic.setCalib(&(cfg.tp_calib));
    SoftPotMagic.setMinGapRatioInt(cfg.tp_gap_ratio);

    // Prevent incompatible value overflows the state
    tp_mode = static_cast<TPMode>(static_cast<uint8_t>(cfg.default_tp_mode) % static_cast<uint8_t>(TPMode::_TOTAL_MODES));

    LCD.begin(16, 2);
    LCD.clear();
    LCD.home();
    redraw_tp_mode();
    if (cfg.ds4_passthrough) {
        LCD.setCursor(5, 1);
        LCD.print("DP");
    }

    // Callback for input scan event
    ScanEvent.attach([](EventResponderRef event) {
        elapsedMicros elapsedTime = 0;
        DS4.update();
        scan_buttons();
        scan_touchpad();
        if (cfg.ds4_passthrough) handle_ds4_pass();
        if (cfg.perf_ctr) {
            etds += elapsedTime;
            sps++;
        }
    });

    // Callback for lower-speed input scan event (for reading rotary encoder values)
    LowSpeedScanEvent.attach([](EventResponderRef event) {
        handle_tp_mode_switch();
    });

    // Background task for USB host library
    USBHTaskEvent.attach([](EventResponderRef event) {
        USBH.Task();
    });

    // Callback for scan/report rate display
    LCDPerfEvent.attach([](EventResponderRef event) {
        if (cfg.perf_ctr) {
            LCD.setCursor(0, 0);
            LCD.print("                ");
            // Reports (frames) per second
            LCD.setCursor(0, 0);
            LCD.print(fps);
            // Scans per second
            LCD.setCursor(5, 0);
            LCD.print(sps);
            // Average time per scan (in microseconds)
            LCD.setCursor(10, 0);
            LCD.print(etds / sps);
            fps = 0;
            sps = 0;
            etds = 0;
        }
    });

    // The lazy-update task for transport
    DS4TUpdateEvent.attach([](EventResponderRef event) {
        DS4T.update();
    });

    // Callback for authentication state change
    // Schedules the update task only when needed
    DS4T.attachStateChangeCallback([](void) {
        DS4TUpdateEvent.triggerEvent();
    });

    // Immediately start scanning
    ScanEvent.triggerEvent();

    // USBH is polled every 1ms
    USBHTaskTimer.beginRepeating(1, USBHTaskEvent);
    // 100ms (10fps) should be okay for rotary encoder since the actual counting is done via interrupt
    LowSpeedScanTimer.beginRepeating(100, LowSpeedScanEvent);
    // Update perf counter every 1 second
    LCDPerfTimer.beginRepeating(1000, LCDPerfEvent);
}

void loop() {
    if (DS4.sendReportBlocking()) {
        if (cfg.perf_ctr) {
            fps++;
        }
        // Trigger a input scan event as soon as one report was sent successfully
        ScanEvent.triggerEvent();
    }
    // Do any unfinished tasks if needed
    yield();
}
