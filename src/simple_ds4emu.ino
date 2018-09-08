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

const uint32_t SCAN_INTERVAL = 1;

ds4_auth_result_t tmp_auth_result = {0};

USB USBH;
USBHub Hub1(&USBH);
PS4USB RealDS4(&USBH);

#if 0
const uint8_t BUTTON_MAP[BUTTONS][2] = {
    {3, DS4_BTN_TRIANGLE},
    {2, DS4_BTN_SQUARE},
    {1, DS4_BTN_CROSS},
    {0, DS4_BTN_CIRCLE},
    {8, DS4_BTN_SHARE},
    {9, DS4_BTN_PS},
    {10, DS4_BTN_OPTION}
};
#endif

const uint8_t DPAD_MAP[4] = {6, 7, 14, 15}; // ULDR

uint8_t lamps;
uint16_t buttons;
uint8_t tp_mode;

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

#if 0
    for (int i=0; i<BUTTONS; i++) {
        // active low
        if (!(buttons & (1 << BUTTON_MAP[i][0]))) {
            DS4.pressButton(BUTTON_MAP[i][1]);
        } else {
            DS4.releaseButton(BUTTON_MAP[i][1]);
        }
    }
#endif
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

#if 0
void scan_dpad() {
    uint8_t tmpstate = 0;
    // RDLU
    for (int i=0; i<4; i++) {
        if (digitalRead(DPAD_MAP[i]) == LOW) {
            tmpstate |= 1 << i;
        }
    }
    switch (tmpstate) {
        case 1:
            DS4.pressDpad(DS4_DPAD_N);
            break;
        case 2:
            DS4.pressDpad(DS4_DPAD_W);
            break;
        case 4:
            DS4.pressDpad(DS4_DPAD_S);
            break;
        case 8:
            DS4.pressDpad(DS4_DPAD_E);
            break;
        case 3: // UL
            DS4.pressDpad(DS4_DPAD_NW);
            break;
        case 6: // LD
            DS4.pressDpad(DS4_DPAD_SW);
            break;
        case 12: // DR
            DS4.pressDpad(DS4_DPAD_SE);
            break;
        case 9: // UR
            DS4.pressDpad(DS4_DPAD_NE);
            break;
        default:
            DS4.releaseDpad();
    }
}
#endif

void handle_auth() {
    static bool lcd_disp = false;
    static uint32_t ts = 0;

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
        Serial1.println("I: authChallengeAvailable");
        NVIC_DISABLE_IRQ(IRQ_USBOTG);
        result = RealDS4.SetReport(0, 0, 0x03, 0xf0, sizeof(ds4_auth_t), (uint8_t *) DS4.authGetChallenge());
        if (result) {
            Serial1.print("E: ");
            Serial1.println(result);
        }
        NVIC_ENABLE_IRQ(IRQ_USBOTG);
    // PS4 is asking for trouble
    } else if (DS4.authChallengeSent()) {
        LCD.setCursor(8, 1);
        LCD.print("W");
        lcd_disp = true;
        ts = millis();
        Serial1.println("I: authChallengeSent");
        result = RealDS4.GetReport(0, 0, 0x03, 0xf2, sizeof(ds4_auth_result_t), (uint8_t *) &tmp_auth_result);
        if (result) {
            Serial1.print("E: ");
            Serial1.println(result);
        }
        if (tmp_auth_result.status == 0x00) {
            Serial1.println("I: RealDS4 ok");
            NVIC_DISABLE_IRQ(IRQ_USBOTG);
            result = RealDS4.GetReport(0, 0, 0x03, 0xf1, sizeof(ds4_auth_t), (uint8_t *) DS4.authGetResponseBuffer());
            if (result) {
                Serial1.print("E: ");
                Serial1.println(result);
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
        Serial1.println("I: authResponseAvailable");
        NVIC_DISABLE_IRQ(IRQ_USBOTG);
        RealDS4.GetReport(0, 0, 0x03, 0xf1, sizeof(ds4_auth_t), (uint8_t *) DS4.authGetResponseBuffer());
        DS4.authSetBufferedFlag();
        NVIC_ENABLE_IRQ(IRQ_USBOTG);
    }
}

void scan_touchpad(void) {
    uint8_t pos1 = POS_FLOAT, pos2 = POS_FLOAT;
    static uint8_t pos1_last_zone = POS_FLOAT, pos2_last_zone = POS_FLOAT;
    static uint8_t pulse1_ctrp = 0, pulse2_ctrp = 0, pulse1_ctrn = 0, pulse2_ctrn = 0;
    static bool pulse1_dir = false, pulse2_dir = false;
    static int8_t pulse1_frame = 0, pulse2_frame = 0;
    uint8_t tmpstate;

    RAL.update();
    RAR.update();
    SoftPotMagic.update();
    pos1 = SoftPotMagic.pos1();
    pos2 = SoftPotMagic.pos2();

    switch (tp_mode) {
        case TP_MODE_TP_A:
        case TP_MODE_TP_C:
        case TP_MODE_TP:
            if (pos1 != POS_FLOAT) {
                if (tp_mode == TP_MODE_TP_A) {
                    //pulse1_frame = 0;
                    if (pos1_last_zone == POS_FLOAT) {
                        pos1_last_zone = pos1 >> 5;
                    } else {
                        int8_t delta = (int8_t) (pos1 >> 5) - pos1_last_zone;
                        if (delta > 0) {
                            pulse1_ctrp += delta;
                        } else if (delta < 0) {
                            pulse1_ctrn += abs(delta);
                        }
                        pos1_last_zone = pos1 >> 5;
                    }
                } else if (tp_mode == TP_MODE_TP_C) {
                    DS4.pressButton(DS4_BTN_TOUCH);
                }
                DS4.setTouchPos1(map(pos1, POS_MIN, POS_MAX, 0, 1919), 471);
                if (pos2 != POS_FLOAT) {
                    //pulse2_frame = 0;
                    if (tp_mode == TP_MODE_TP_A) {
                        if (pos2_last_zone == POS_FLOAT) {
                            pos2_last_zone = pos2 >> 5;
                        } else {
                            int8_t delta = (int8_t) (pos2 >> 5) - pos2_last_zone;
                            if (delta > 0) {
                                pulse2_ctrp += delta;
                            } else if (delta < 0) {
                                pulse2_ctrn += abs(delta);
                            }
                            pos2_last_zone = pos2 >> 5;
                        }
                    }
                    DS4.setTouchPos2(map(pos2, POS_MIN, POS_MAX, 0, 1919), 471);
                } else {
                    DS4.releaseTouchPos2();
                    //pulse2_frame = -1;
                    pos2_last_zone = POS_FLOAT;
                }
            } else {
                if (tp_mode == TP_MODE_TP_C) {
                    DS4.releaseButton(DS4_BTN_TOUCH);
                }
                DS4.releaseTouchAll();
                //pulse1_frame = -1;
                //pulse2_frame = -1;
                pos1_last_zone = POS_FLOAT;
                pos2_last_zone = POS_FLOAT;
            }
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

    if (pulse1_frame == 0) {
        if (pulse1_ctrp > 0 && (pulse1_ctrn == 0 || pulse1_dir)) {
            pulse1_ctrp--;
            DS4.setLeftAnalog(255, 127);
        } else if (pulse1_ctrn > 0 && (pulse1_ctrp == 0 || !pulse1_dir)) {
            pulse1_ctrn--;
            DS4.setLeftAnalog(0, 127);
        } else if (pulse1_ctrn == 0 && pulse1_ctrp == 0) {
            DS4.setLeftAnalog(127, 127);
        }
        pulse1_dir = !pulse1_dir;
    }
    if (pulse1_frame >= 0) {
        pulse1_frame++;
        pulse1_frame %= 24;
    }

    if (pulse2_frame == 0) {
        if (pulse2_ctrp > 0 && (pulse2_ctrn == 0 || pulse2_dir)) {
            pulse2_ctrp--;
            DS4.setRightAnalog(255, 127);
        } else if (pulse2_ctrn > 0 && (pulse2_ctrp == 0 || !pulse2_dir)) {
            pulse2_ctrn--;
            DS4.setRightAnalog(0, 127);
        } else if (pulse2_ctrn == 0 && pulse2_ctrp == 0) {
            DS4.setRightAnalog(127, 127);
        }
        pulse2_dir = !pulse2_dir;
    }
    if (pulse2_frame >= 0) {
        pulse2_frame++;
        pulse2_frame %= 24;
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
        case TP_MODE_TP_A:
            LCD.print(TP_MODE_TP_A_N);
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

void setup() {
    Serial1.begin(115200);
    Serial1.println("I: Hello");

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
    Serial1.println("I: init usb");
    DS4.begin();

    Serial1.println("I: init usbh");
    if (USBH.Init() == -1) {
        Serial1.println("F: timeout waiting for usbh");
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

    tp_mode = controller_settings.default_tp_mode;

    LCD.begin(16, 2);
    LCD.clear();
    LCD.home();
    redraw_tp_mode();
    if (controller_settings.ds4_passthrough) {
        LCD.setCursor(5, 1);
        LCD.print("DP");
    }
}

void loop() {
    // Poll the USBH controller
    static uint16_t fps = 0, vps = 0;
    static uint32_t perf = millis();
    static uint32_t elapsed = 0;

    USBH.Task();
    DS4.update();
    if (millis() - elapsed >= SCAN_INTERVAL) {
        elapsed = millis();
        scan_buttons();
        scan_touchpad();
        if (controller_settings.perf_ctr) {
            if (DS4.sendAsync()) {
                fps++;
            }
            vps++;
        } else {
            DS4.sendAsync();
        }
    }

    handle_auth();
    handle_tp_mode_switch();
    if (controller_settings.ds4_passthrough) handle_ds4_pass();
    if (controller_settings.perf_ctr && millis() - perf >= 1000) {
        perf = millis();
        LCD.setCursor(0, 0);
        LCD.print("                ");
        LCD.setCursor(0, 0);
        LCD.print(fps);
        LCD.setCursor(8, 0);
        LCD.print(vps);
        fps = 0;
        vps = 0;
    }
}
