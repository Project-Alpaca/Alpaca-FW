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
#include "settings.h"
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

USB USBH;
USBHub Hub1(&USBH);
ds4::PS4USB2 RealDS4(&USBH);

ds4::AuthenticatorUSBH DS4A(&RealDS4);
ds4::TransportTeensy DS4T(&DS4A);
ds4::ControllerSOCD<> DS4(&DS4T);

EventResponder ScanEvent;
EventResponder USBHTaskEvent;
EventResponder LowSpeedScanEvent;
EventResponder LCDPerfEvent;
EventResponder DS4TUpdateEvent;
MillisTimer USBHTaskTimer;
MillisTimer LowSpeedScanTimer;
MillisTimer LCDPerfTimer;

uint8_t lamps;
uint16_t buttons;
uint8_t tp_mode;
// Scans-per-second counter
static uint16_t sps = 0;
static uint16_t fps = 0;

static inline void scan_buttons() {
    uint8_t lamps_new;

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

    for (uint8_t i=0; i<16; i++) {
        // TODO this is hacked to work (for now). We need to find a way to refactor this.
        if (ISBTN(controller_settings.button_mapping[i])) {
            if (!(buttons & (1 << i))) {
                DS4.pressKey(BTN2DS4(controller_settings.button_mapping[i]));
            } else {
                DS4.releaseKey(BTN2DS4(controller_settings.button_mapping[i]));
            }
        }
    }
}

void handle_touchpad_direct_mapping(uint8_t pos1, uint8_t pos2, bool click) {
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

void handle_touchpad_atrf(uint8_t pos1, uint8_t pos2) {
    static uint8_t pos1_prev = POS_FLOAT, pos2_prev = POS_FLOAT;
    static uint8_t stick_hold_frames = 0;
    static bool released = true;
    static const int16_t tp_max = ATRF_SEG_WIDTH * controller_settings.max_segs - 1;

    bool is_chain_slide = (DS4.getRumbleIntensityRight() > 0);
    bool dir_changed = false;

    // Start fresh
    DS4.clearTouchEvents();

    if (is_chain_slide) {
        // immediately clear stick states
        stick_hold_frames = 0;
        if (pos1 != POS_FLOAT) {
            // Map both points to pos1
            // TODO what if 2 points are moving towards different directions?
            DS4.setTouchEvent(0, true, map(pos1, POS_MIN, POS_MAX, 0, tp_max), 314);
            if (controller_settings.seg_mult) {
                DS4.setTouchEvent(1, true, map(pos1, POS_MIN, POS_MAX, 0, tp_max), 628);
            }
            DS4.finalizeTouchEvent();
        }
        // otherwise, do nothing since the touch is already released
    } else {
        // starts from an untouched touchpad
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
            stick_hold_frames = controller_settings.stick_hold;
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

static inline void scan_touchpad(void) {
    uint8_t pos1 = POS_FLOAT, pos2 = POS_FLOAT;
    RAL.update();
    RAR.update();
    SoftPotMagic.update();
    pos1 = SoftPotMagic.pos1();
    pos2 = SoftPotMagic.pos2();

    switch (tp_mode) {
        case TP_MODE_ATRF:
            handle_touchpad_atrf(pos1, pos2);
            break;
        case TP_MODE_TP_C:
        case TP_MODE_TP:
            handle_touchpad_direct_mapping(pos1, pos2, tp_mode == TP_MODE_TP_C);
            break;
        // ULRD
        case TP_MODE_DPAD: {
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
            if (pos1 != 4) tmpstate |= 1 << pos1;
            if (pos2 != 4) tmpstate |= 1 << pos2;
            DS4.setDpadUniversalSOCD(tmpstate & (1 << 0), tmpstate & (1 << 2), tmpstate & (1 << 3), tmpstate & (1 << 1));
            break;
        }
        case TP_MODE_LR:
            // TODO
            DS4.clearTouchEvents();
            break;
    }
}

void redraw_tp_mode(void) {
    LCD.setCursor(0, 1);
    LCD.print("    ");
    LCD.setCursor(0, 1);
    switch (tp_mode) {
        case TP_MODE_TP:
            LCD.print(TP_MODE_TP_N);
            break;
        case TP_MODE_DPAD:
            LCD.print(TP_MODE_DPAD_N);
            break;
        case TP_MODE_LR:
            LCD.print(TP_MODE_LR_N);
            break;
        case TP_MODE_TP_C:
            LCD.print(TP_MODE_TP_C_N);
            break;
        case TP_MODE_ATRF:
            LCD.print(TP_MODE_ATRF_N);
            break;
    }
}

void handle_tp_mode_switch(void) {
    long qei_step = QEI.read() / 4;
    if (qei_step != 0) {
        if (qei_step < 0 && tp_mode < -qei_step) {
            tp_mode += (TP_NB_MODES + qei_step);
        } else {
            tp_mode += qei_step;
        }

        tp_mode %= TP_NB_MODES;
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

    settings_load(&controller_settings);

    if (controller_settings.tp_calib.leftMin == -1 || controller_settings.tp_calib.rightMin == -1) {
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
    SoftPotMagic.setCalib(&(controller_settings.tp_calib));
    SoftPotMagic.setMinGapRatio(.10f);

    tp_mode = controller_settings.default_tp_mode % TP_NB_MODES;

    LCD.begin(16, 2);
    LCD.clear();
    LCD.home();
    redraw_tp_mode();
    if (controller_settings.ds4_passthrough) {
        LCD.setCursor(5, 1);
        LCD.print("DP");
    }

    ScanEvent.attach([](EventResponderRef event) {
        DS4.update();
        scan_buttons();
        scan_touchpad();
        if (controller_settings.ds4_passthrough) handle_ds4_pass();
        if (controller_settings.perf_ctr) sps++;
    });

    LowSpeedScanEvent.attach([](EventResponderRef event) {
        handle_tp_mode_switch();
    });

    USBHTaskEvent.attach([](EventResponderRef event) {
        USBH.Task();
    });

    LCDPerfEvent.attach([](EventResponderRef event) {
        if (controller_settings.perf_ctr) {
            LCD.setCursor(0, 0);
            LCD.print("                ");
            LCD.setCursor(0, 0);
            LCD.print(fps);
            LCD.setCursor(8, 0);
            LCD.print(sps);
            fps = 0;
            sps = 0;
        }
    });

    // The lazy-update task for transport
    DS4TUpdateEvent.attach([](EventResponderRef event) {
        DS4T.update();
    });

    // Schedules the update task only when needed
    DS4T.attachStateChangeCallback([](void) {
        DS4TUpdateEvent.triggerEvent();
    });

    ScanEvent.triggerEvent();

    USBHTaskTimer.beginRepeating(1, USBHTaskEvent);
    LowSpeedScanTimer.beginRepeating(100, LowSpeedScanEvent);
    LCDPerfTimer.beginRepeating(1000, LCDPerfEvent);
}

void loop() {
    if (DS4.sendReportBlocking()) {
        if (controller_settings.perf_ctr) {
            fps++;
        }
        ScanEvent.triggerEvent();
    }
    yield();
}
