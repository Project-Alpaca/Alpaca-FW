#include <usbhub.h>
#include <PS4USB.h>
#include <SoftPotMagic.h>

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>
#include <Encoder.h>
#include <LiquidCrystal.h>
#include <ResponsiveAnalogRead.h>

#include "service_menu.h"
#include "common_objs.h"
#include "settings.h"
#include "constants.h"

// Debug info
#if defined(DS4_DEBUG_INFO) && DS4_DEBUG_INFO == 1
    #define DEBUG_CONSOLE_INIT() Serial1.begin(115200)
    #define DEBUG_CONSOLE_PRINT(args) Serial1.print(args)
    #define DEBUG_CONSOLE_PRINTLN(args) Serial1.println(args)
#else
    #define DEBUG_CONSOLE_INIT() while (0) {}
    #define DEBUG_CONSOLE_PRINT(args) while (0) {}
    #define DEBUG_CONSOLE_PRINTLN(args) while (0) {}
#endif

const uint32_t SCAN_INTERVAL_US = 1000;

ds4_auth_result_t tmp_auth_result = {0};

const uint16_t HORI_VID = 0x0f0d;
const uint16_t HORI_PID_MINI = 0x00ee;

class PS4USB2: public PS4USB {
public:
    PS4USB2(USB *p) : PS4USB(p) {};

    bool connected() {
        uint16_t v = HIDUniversal::VID;
        uint16_t p = HIDUniversal::PID;
        return HIDUniversal::isReady() && (( \
            v == PS4_VID && ( \
                p == PS4_PID || \
                p == PS4_PID_SLIM \
            )
        ) || ( \
            v == HORI_VID && ( \
                p == HORI_PID_MINI \
            ) \
        ));
    }

    bool isLicensed(void) {
        return HIDUniversal::VID != PS4_VID;
    }
};

USB USBH;
USBHub Hub1(&USBH);
PS4USB2 RealDS4(&USBH);
IntervalTimer Scan;

const uint8_t DPAD_MAP[4] = {6, 7, 14, 15}; // ULDR

uint8_t lamps;
uint16_t buttons;
uint8_t tp_mode;
// Scans-per-second conter
volatile uint16_t sps = 0;

void scan_buttons() {
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
        if (ISBTN(controller_settings.button_mapping[i])) {
            if (!(buttons & (1 << i))) {
                DS4.pressButton(BTN2DS4(controller_settings.button_mapping[i]));
            } else {
                DS4.releaseButton(BTN2DS4(controller_settings.button_mapping[i]));
            }
        }
    }
}

void handle_auth() {
    static bool lcd_disp = false;
    static uint32_t ts = 0;
    uint8_t reset_buffer[8];

    if (!RealDS4.connected()) {
        // Display warning and latches on until DS4 is available
        if (!lcd_disp) {
            LCD.setCursor(8, 1);
            LCD.print("!");
            lcd_disp = true;
            ts = millis();
        }
        return;
    }

    int result;
    if (lcd_disp && millis() - ts >= 250) {
        LCD.setCursor(8, 1);
        LCD.print(" ");
        lcd_disp = false;
    }

    // PS4 is spitting nonsense
    if (DS4.authChallengeAvailable()) {
        LCD.setCursor(8, 1);
        LCD.print("Q");
        lcd_disp = true;
        ts = millis();
        DEBUG_CONSOLE_PRINTLN("I: authChallengeAvailable");
        if (RealDS4.isLicensed() && DS4.authGetChallenge()->page == 0) {
            DEBUG_CONSOLE_PRINTLN("I: licensed controller, resetting");
            NVIC_DISABLE_IRQ(IRQ_PIT);
            result = RealDS4.GetReport(0, 0, 0x03, 0xf3, sizeof(reset_buffer), reset_buffer);
            NVIC_ENABLE_IRQ(IRQ_PIT);
            if (result) {
                DEBUG_CONSOLE_PRINT("E: ");
                DEBUG_CONSOLE_PRINTLN(result);
            }
        }
        NVIC_DISABLE_IRQ(IRQ_USBOTG);
        NVIC_DISABLE_IRQ(IRQ_PIT);
        result = RealDS4.SetReport(0, 0, 0x03, 0xf0, sizeof(ds4_auth_t), (uint8_t *) DS4.authGetChallenge());
        NVIC_ENABLE_IRQ(IRQ_PIT);
        if (result) {
            DEBUG_CONSOLE_PRINT("E: ");
            DEBUG_CONSOLE_PRINTLN(result);
        }
        NVIC_ENABLE_IRQ(IRQ_USBOTG);

    // PS4 is asking for trouble
    } else if (DS4.authChallengeSent()) {
        LCD.setCursor(8, 1);
        LCD.print("W");
        lcd_disp = true;
        ts = millis();
        DEBUG_CONSOLE_PRINTLN("I: authChallengeSent");
        NVIC_DISABLE_IRQ(IRQ_PIT);
        result = RealDS4.GetReport(0, 0, 0x03, 0xf2, sizeof(ds4_auth_result_t), (uint8_t *) &tmp_auth_result);
        NVIC_ENABLE_IRQ(IRQ_PIT);
        if (result) {
            DEBUG_CONSOLE_PRINT("E: ");
            DEBUG_CONSOLE_PRINTLN(result);
        }
        if (tmp_auth_result.status == 0x00) {
            DEBUG_CONSOLE_PRINTLN("I: RealDS4 ok");
            NVIC_DISABLE_IRQ(IRQ_USBOTG);
            NVIC_DISABLE_IRQ(IRQ_PIT);
            result = RealDS4.GetReport(0, 0, 0x03, 0xf1, sizeof(ds4_auth_t), (uint8_t *) DS4.authGetResponseBuffer());
            NVIC_ENABLE_IRQ(IRQ_PIT);
            if (result) {
                DEBUG_CONSOLE_PRINT("E: ");
                DEBUG_CONSOLE_PRINTLN(result);
            }
            DS4.authSetBufferedFlag();
            NVIC_ENABLE_IRQ(IRQ_USBOTG);
        }
    // PS4 is blaming you for messing things up
    } else if (DS4.authResponseAvailable()) {
        LCD.setCursor(8, 1);
        LCD.print("R");
        lcd_disp = true;
        ts = millis();
        DEBUG_CONSOLE_PRINTLN("I: authResponseAvailable");
        NVIC_DISABLE_IRQ(IRQ_USBOTG);
        NVIC_DISABLE_IRQ(IRQ_PIT);
        RealDS4.GetReport(0, 0, 0x03, 0xf1, sizeof(ds4_auth_t), (uint8_t *) DS4.authGetResponseBuffer());
        NVIC_ENABLE_IRQ(IRQ_PIT);
        DS4.authSetBufferedFlag();
        NVIC_ENABLE_IRQ(IRQ_USBOTG);
    }
}

void handle_touchpad_direct_mapping(uint8_t pos1, uint8_t pos2, bool click) {
    if (pos1 != POS_FLOAT) {
        if (click) {
            DS4.pressButton(DS4_BTN_TOUCH);
        }
        DS4.setTouchPos1(map(pos1, POS_MIN, POS_MAX, 0, 1919), 471);
        if (pos2 != POS_FLOAT) {
            DS4.setTouchPos2(map(pos2, POS_MIN, POS_MAX, 0, 1919), 471);
        } else {
            DS4.releaseTouchPos2();
        }
    } else {
        if (click) {
            DS4.releaseButton(DS4_BTN_TOUCH);
        }
        DS4.releaseTouchAll();
    }
}

void handle_touchpad_atrf(uint8_t pos1, uint8_t pos2) {
    static uint8_t pos1_prev = POS_FLOAT, pos2_prev = POS_FLOAT;
    static uint8_t stick_hold_frames = 0;
    static bool released = true;
    static const int16_t tp_max = ATRF_SEG_WIDTH * controller_settings.max_segs - 1;

    bool is_long_slider = (DS4.getRumbleStrengthRight() > 0);
    bool dir_changed = false;

    if (is_long_slider) {
        // immediately clear stick states
        stick_hold_frames = 0;
        if (pos1 != POS_FLOAT) {
            // Map both points to pos1
            // TODO what if 2 points are moving towards different directions?
            DS4.setTouchPos1(map(pos1, POS_MIN, POS_MAX, 0, tp_max), 314);
            if (controller_settings.seg_mult) {
                DS4.setTouchPos2(map(pos1, POS_MIN, POS_MAX, 0, tp_max), 628);
            }
        } else {
            DS4.releaseTouchAll();
        }
    } else {
        // immediately release touchpad
        DS4.releaseTouchAll();
        // if slider is being touched
        if (pos1 != POS_FLOAT) {
            // and we can determine the direction
            if (pos1_prev != POS_FLOAT) {
                // left
                if (pos1 < pos1_prev) {
                    DS4.setLeftAnalog(0, 127);
                    dir_changed = true;
                // right
                } else if (pos1 > pos1_prev) {
                    DS4.setLeftAnalog(255, 127);
                    dir_changed = true;
                }
            }
            // same for the second touch point
            if (pos2 != POS_FLOAT && pos2_prev != POS_FLOAT) {
                // left
                if (pos2 < pos2_prev) {
                    DS4.setRightAnalog(0, 127);
                    dir_changed = true;
                // right
                } else if (pos2 > pos2_prev) {
                    DS4.setRightAnalog(255, 127);
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
        DS4.setLeftAnalog(127, 127);
        DS4.setRightAnalog(127, 127);
        released = true;
    }
    if (stick_hold_frames > 0) stick_hold_frames--;
    pos1_prev = pos1;
    pos2_prev = pos2;
}

void scan_touchpad(void) {
    uint8_t pos1 = POS_FLOAT, pos2 = POS_FLOAT;
    uint8_t tmpstate;

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
        case TP_MODE_DPAD:
            DS4.releaseTouchAll();
            tmpstate = 0;
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

            switch (tmpstate) {
                case 1:
                    DS4.pressDpad(DS4_DPAD_N);
                    break;
                case 2:
                    DS4.pressDpad(DS4_DPAD_W);
                    break;
                case 4:
                    DS4.pressDpad(DS4_DPAD_E);
                    break;
                case 8:
                    DS4.pressDpad(DS4_DPAD_S);
                    break;
                case 3:
                    DS4.pressDpad(DS4_DPAD_NW);
                    break;
                case 5:
                    DS4.pressDpad(DS4_DPAD_NE);
                    break;
                case 10:
                    DS4.pressDpad(DS4_DPAD_SW);
                    break;
                case 12:
                    DS4.pressDpad(DS4_DPAD_SE);
                    break;
                case 0:
                default:
                    DS4.releaseDpad();
            }
            break;
        case TP_MODE_LR:
            // TODO
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
        QEI.write(0);
    }
}

void handle_ds4_pass(void) {
    if (!RealDS4.connected()) return;

    // axis to buttons to axis, really? D:
    if (RealDS4.getButtonPress(UP)) {
        if (RealDS4.getButtonPress(LEFT)) {
            DS4.pressDpad(DS4_DPAD_NW);
        } else if (RealDS4.getButtonPress(RIGHT)) {
            DS4.pressDpad(DS4_DPAD_NE);
        } else {
            DS4.pressDpad(DS4_DPAD_N);
        }
    } else if (RealDS4.getButtonPress(DOWN)) {
        if (RealDS4.getButtonPress(LEFT)) {
            DS4.pressDpad(DS4_DPAD_SW);
        } else if (RealDS4.getButtonPress(RIGHT)) {
            DS4.pressDpad(DS4_DPAD_SE);
        } else {
            DS4.pressDpad(DS4_DPAD_S);
        }
    } else if (RealDS4.getButtonPress(LEFT)) {
        DS4.pressDpad(DS4_DPAD_W);
    } else if (RealDS4.getButtonPress(RIGHT)) {
        DS4.pressDpad(DS4_DPAD_E);
    } else {
        DS4.releaseDpad();
    }

    // buttons TODO the rest of them
    if (RealDS4.getButtonPress(L1)) {
        DS4.pressButton(DS4_BTN_L1);
    } else {
        DS4.releaseButton(DS4_BTN_L1);
    }
    if (RealDS4.getButtonPress(R1)) {
        DS4.pressButton(DS4_BTN_R1);
    } else {
        DS4.releaseButton(DS4_BTN_R1);
    }
    if (RealDS4.getButtonPress(L2)) {
        DS4.pressButton(DS4_BTN_L2);
    } else {
        DS4.releaseButton(DS4_BTN_L2);
    }
    if (RealDS4.getButtonPress(R2)) {
        DS4.pressButton(DS4_BTN_R2);
    } else {
        DS4.releaseButton(DS4_BTN_R2);
    }

    // analog TODO L2R2
    DS4.setLeftAnalog(RealDS4.getAnalogHat(LeftHatX), RealDS4.getAnalogHat(LeftHatY));
    DS4.setRightAnalog(RealDS4.getAnalogHat(RightHatX), RealDS4.getAnalogHat(RightHatY));
}

void _scan_all() {
    scan_buttons();
    scan_touchpad();
    if (controller_settings.perf_ctr) sps++;
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
    DEBUG_CONSOLE_PRINTLN("I: init usb");
    DS4.begin();

    DEBUG_CONSOLE_PRINTLN("I: init usbh");
    if (USBH.Init() == -1) {
        DEBUG_CONSOLE_PRINTLN("F: timeout waiting for usbh");
        while (1);
    }

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

    Scan.begin(_scan_all, SCAN_INTERVAL_US);
}

void loop() {
    // Poll the USBH controller
    static uint16_t fps = 0;
    static uint32_t perf = millis();

    NVIC_DISABLE_IRQ(IRQ_PIT);
    USBH.Task();
    DS4.update();
    if (controller_settings.perf_ctr) {
        if (DS4.sendAsync()) {
            fps++;
        }
    } else {
        DS4.sendAsync();
    }
    NVIC_ENABLE_IRQ(IRQ_PIT);

    handle_auth();
    handle_tp_mode_switch();
    if (controller_settings.ds4_passthrough) handle_ds4_pass();

    if (controller_settings.perf_ctr && millis() - perf >= 1000) {
        perf = millis();
        LCD.setCursor(0, 0);
        LCD.print("                ");
        NVIC_DISABLE_IRQ(IRQ_PIT);
        LCD.setCursor(0, 0);
        LCD.print(fps);
        LCD.setCursor(8, 0);
        LCD.print(sps);
        fps = 0;
        sps = 0;
        NVIC_ENABLE_IRQ(IRQ_PIT);
    }
}
