#include <Arduino.h>
#include <SPI.h>
#include <Encoder.h>
#include <LiquidCrystalNew.h>
#include <MD_Menu.h>
#include <ResponsiveAnalogRead.h>
#include <SoftPotMagic.h>

#include "service_menu.h"
#include "common_objs.h"
#include "settings.h"
#include "constants.h"

bool disp_control(MD_Menu::userDisplayAction_t action, char *msg = nullptr);
MD_Menu::userNavAction_t input_control(uint16_t &step);

MD_Menu::value_t *io_test_wrapper(MD_Menu::mnuId_t id, bool get);
MD_Menu::value_t *tp_calib_wrapper(MD_Menu::mnuId_t id, bool get);
MD_Menu::value_t *reboot_wrapper(MD_Menu::mnuId_t id, bool get);
MD_Menu::value_t *clear_eeprom_wrapper(MD_Menu::mnuId_t id, bool get);
MD_Menu::value_t *handle_list(MD_Menu::mnuId_t id, bool get);
MD_Menu::value_t *handle_int(MD_Menu::mnuId_t id, bool get);
MD_Menu::value_t *handle_bool(MD_Menu::mnuId_t id, bool get);

MD_Menu::value_t menu_buf;

#ifndef TP_RESISTIVE
#define TP_RESISTIVE
#endif

const int MENU_TP_MIN = 100;

#ifdef TP_RESISTIVE
const int MENU_TP_MAX = 101;
#endif

const MD_Menu::mnuHeader_t menus[] = {
    // id desc lower_item upper_item curr_item
    {10, "Service Menu", 10, 17, 0},
    {11, "Reboot", 20, 21, 0},
    {13, "I/O Test", 40, 41, 0},
    {14, "TP Config", 50, 53, 0},
    {20, "TP SysConf", MENU_TP_MIN, MENU_TP_MAX, 0},
};

const MD_Menu::mnuItem_t menu_items[] = {
    // id desc item_type item_id
    {10, "I/O Test...", MD_Menu::MNU_MENU, 13},
    {11, "TP Conf...", MD_Menu::MNU_MENU, 14},
    {12, "TP SysConf...", MD_Menu::MNU_MENU, 20},
    {13, "DS4 Redir.", MD_Menu::MNU_INPUT, 16},
    {14, "Show PerfCtr.", MD_Menu::MNU_INPUT, 17},
    {15, "Clear EEPROM", MD_Menu::MNU_INPUT, 18},
    {16, "Reboot...", MD_Menu::MNU_MENU, 11},

    {20, "Main System", MD_Menu::MNU_INPUT, 20}, // action 20 resets the MCU
    {21, "Bootloader", MD_Menu::MNU_INPUT, 21}, // action 21 reboots into bootloader


    {40, "Button Test", MD_Menu::MNU_INPUT, 10},
    {41, "TP Test", MD_Menu::MNU_INPUT, 11},

    {50, "TP Mode", MD_Menu::MNU_INPUT, 13},
    {51, "ATRF Hold", MD_Menu::MNU_INPUT, 19},
    {52, "ATRF MaxSeg", MD_Menu::MNU_INPUT, 40},
    {53, "ATRF SegMult", MD_Menu::MNU_INPUT, 41},
#ifdef TP_RESISTIVE
    {100, "TP Calibration", MD_Menu::MNU_INPUT, 12}, // action 12 runs the calibration function
    {101, "TP ADC Zero", MD_Menu::MNU_INPUT, 15},
#endif
};

const MD_Menu::mnuInput_t menu_actions[] = {
    // id desc type handler field_size lower_n lower_e upper_n upper_e list_items
    {10, "Press SEL", MD_Menu::INP_RUN, io_test_wrapper, 0, 0, 0, 0, 0, 0, nullptr},
    {11, "Press SEL", MD_Menu::INP_RUN, io_test_wrapper, 0, 0, 0, 0, 0, 0, nullptr},
    {12, "Press SEL", MD_Menu::INP_RUN, tp_calib_wrapper, 0, 0, 0, 0, 0, 0, nullptr},
    {13, "Mode", MD_Menu::INP_LIST, handle_list, 4, 0, 0, 0, 0, 0, TP_MODES},
    {15, "Thres.", MD_Menu::INP_INT, handle_int, 4, 0, 0, 1023, 0, 10, nullptr},
    {16, "Y/N", MD_Menu::INP_BOOL, handle_bool, 1, 0, 0, 0, 0, 0, nullptr},
    {17, "Y/N", MD_Menu::INP_BOOL, handle_bool, 1, 0, 0, 0, 0, 0, nullptr},
    {18, "Clear EEPROM?", MD_Menu::INP_RUN, clear_eeprom_wrapper, 0, 0, 0, 0, 0, 0, nullptr},
    {19, "Scans", MD_Menu::INP_INT, handle_int, 3, 0, 0, 255, 0, 10, nullptr},
    {20, "Restart?", MD_Menu::INP_RUN, reboot_wrapper, 0, 0, 0, 0, 0, 0, nullptr},
    {21, "Reboot to BL?", MD_Menu::INP_RUN, reboot_wrapper, 0, 0, 0, 0, 0, 0, nullptr},
    {40, "Segments", MD_Menu::INP_INT, handle_int, 2, 1, 0, 20, 0, 10, nullptr},
    {41, "Multiplier", MD_Menu::INP_INT, handle_int, 2, 1, 0, 2, 0, 10, nullptr},
};

MD_Menu TestMenu(
    input_control, disp_control,
    menus, ARRAY_SIZE(menus),
    menu_items, ARRAY_SIZE(menu_items),
    menu_actions, ARRAY_SIZE(menu_actions)
);

static bool qei_sw_block;
static int8_t qei_sw_state;
static uint32_t qei_sw_hold;
static int8_t _task;

#define LCD_UPDATE_INTERVAL 100

#define TASK_MENU 1
#define TASK_BTNTEST 2
#define TASK_TPTEST 3
#define TASK_TPCALIB 10

#define LONG_PRESS_THRESHOLD 500

void scan_qei_sw(void) {
    if (digitalRead(QEI_SW) == LOW && !qei_sw_block) {
        // starts fresh
        if (qei_sw_state == 0) {
            qei_sw_state = -1;
            qei_sw_hold = millis();
        // still holding
        } else if (qei_sw_state == -1 && (millis() - qei_sw_hold > LONG_PRESS_THRESHOLD)) {
            qei_sw_state = -2;
        }
    } else if (digitalRead(QEI_SW) == HIGH) {
        if (qei_sw_block) {
            qei_sw_block = false;
        // was holding
        } else if (qei_sw_state < 0) {
            qei_sw_state = -qei_sw_state;
        }
    }
}

void reset_qei_sw(void) {
    qei_sw_state = 0;
    qei_sw_hold = 0;
}

void button_test(void) {
    static uint32_t lcd_last_update = 0;
    static bool transition = true;
    uint16_t buttons;
    char buf[17] = {0};

    if (transition) {
        LCD.clear();
        LCD.home();
        LCD.print("Button Test");
        transition = false;
    }

    // if QEI_SW got pressed, return to menu
    if (qei_sw_state > 0) {
        reset_qei_sw();
        _task = TASK_MENU;
        TestMenu.reset();
        transition = true;
        return;
    }

    if ((millis() - lcd_last_update) >= LCD_UPDATE_INTERVAL) {
        lcd_last_update = millis();

        digitalWrite(BTN_CSB, LOW);
        SPI.beginTransaction(SPI4021);
        buttons = SPI.transfer(0x00);
        buttons |= SPI.transfer(0x00) << 8;
        digitalWrite(BTN_CSB, HIGH);
        SPI.endTransaction();
        
        for (int8_t i=0; i<16; i++) {
            buf[i] = ((buttons & (1 << i)) ? '-' : 'X');
        }

        Serial1.println(buf);
        LCD.setCursor(0, 1);
        LCD.print(buf);
    }
}

void tp_test(void) {
    static uint32_t lcd_last_update = 0;
    static bool transition = true;

    if (transition) {
        LCD.clear();
        LCD.home();
        LCD.print("TP Test");
        transition = false;
    }

    // if QEI_SW got pressed, return to menu
    if (qei_sw_state > 0) {
        reset_qei_sw();
        _task = TASK_MENU;
        TestMenu.reset();
        transition = true;
        return;
    }

    if ((millis() - lcd_last_update) >= LCD_UPDATE_INTERVAL) {
        lcd_last_update = millis();

        SoftPotMagic.update();

        LCD.setCursor(0, 1);
        LCD.print("                ");
        LCD.setCursor(0, 1);
        LCD.print(SoftPotMagic.pos1());
        LCD.print(" ");
        LCD.print(SoftPotMagic.pos2());
        LCD.print(" ");
        LCD.print(SoftPotMagic.leftADC());
        LCD.print(" ");
        LCD.print(SoftPotMagic.rightADC());
    }
}

void tp_calib(void) {
    static bool transition = true;
    static int8_t calib_state = 0;
    bool calib_result;
    const calib_t *c;
    calib_t *s;

    if (transition) {
        LCD.clear();
        LCD.home();
        LCD.print("TP Calibration");
        transition = false;
    }

    // abort by holding QEI_SW
    if (qei_sw_state == 2) {
        reset_qei_sw();
        _task = TASK_MENU;
        TestMenu.reset();
        transition = true;
        calib_state = 0;
        return;
    }

    // state machine, or event driven, whatever you want to call it
    switch (calib_state) {
        // Left not calibrated, not prompted
        case 0:
            LCD.setCursor(0, 1);
            LCD.print("Calib left");
            calib_state++;
            break;
        // Left not calibrated, prompted
        case 1:
            if (qei_sw_state == 1) {
                reset_qei_sw();
                calib_result = SoftPotMagic.autoCalibLeft();
                if (calib_result) {
                    calib_state++;
                }
            }
            break;
        case 2:
            LCD.setCursor(0, 1);
            LCD.print("Calib right");
            calib_state++;
            break;
        case 3:
            if (qei_sw_state == 1) {
                reset_qei_sw();
                calib_result = SoftPotMagic.autoCalibRight();
                if (calib_result) {
                    calib_state++;
                }
            }
            break;
        case 4:
            LCD.setCursor(0, 1);
            LCD.print("Start calib zero");
            calib_state++;
            break;
        case 5:
            if (qei_sw_state == 1) {
                reset_qei_sw();
                LCD.setCursor(0, 1);
                LCD.print("Calib zero...");
                calib_result = SoftPotMagic.autoCalibZero();
                if (calib_result) {
                    calib_state++;
                } else {
                    LCD.setCursor(0, 1);
                    LCD.print("Failed!");
                }
            }
            break;
        case 6:
            LCD.setCursor(0, 1);
            LCD.print("Saving...  ");
            s = &(cfg.tp_calib);
            c = SoftPotMagic.getCalib();
            memcpy(s, c, sizeof(cfg.tp_calib));
            cfg.save();
            LCD.setCursor(0, 1);
            LCD.print("Done. Press SEL");
            calib_state++;
            break;
        case 7:
            if (qei_sw_state == 1) {
                reset_qei_sw();
                _task = TASK_MENU;
                TestMenu.reset();
                transition = true;
                calib_state = 0;
            }
            break;
    }
}

bool disp_control(MD_Menu::userDisplayAction_t action, char *msg) {
    static char _blank[LCD_COLS+1] = {0};
    switch (action) {
        case MD_Menu::DISP_INIT:
            LCD.begin(16, 2);
            LCD.clear();
            LCD.noCursor();
            memset(_blank, ' ', LCD_COLS);
            break;
        case MD_Menu::DISP_CLEAR:   
            LCD.clear();
            break;
        case MD_Menu::DISP_L0:
            LCD.setCursor(0, 0);
            LCD.print(_blank);
            LCD.setCursor(0, 0);
            LCD.print(msg);
            break;
        case MD_Menu::DISP_L1:
            LCD.setCursor(0, 1);
            LCD.print(_blank);
            LCD.setCursor(0, 1);
            LCD.print(msg);
            break;
    }
    return true;
}

MD_Menu::userNavAction_t input_control(uint16_t &step) {
    long qei_step = QEI.read() / 4;
    MD_Menu::userNavAction_t action = MD_Menu::NAV_NULL;

    // short hold
    if (qei_sw_state > 0) {
        step = 1;
        if (qei_sw_state == 1) {
            action = MD_Menu::NAV_SEL;
        // long hold
        } else if (qei_sw_state == 2) {
            action = MD_Menu::NAV_ESC;
        }
        reset_qei_sw();
    } else if (qei_step > 0) {
        step = (qei_step > 0xffff) ? 0xffff : qei_step;
        action = MD_Menu::NAV_INC;
        QEI.write(0);
    } else if (qei_step < 0) {
        step = (qei_step < -0xffff) ? 0xffff : -qei_step;
        action = MD_Menu::NAV_DEC;
        QEI.write(0);
    }
    return action;
}

MD_Menu::value_t *handle_list(MD_Menu::mnuId_t id, bool get) {
    switch (id) {
        case 13:
            if (get) {
                menu_buf.value = static_cast<uint8_t>(cfg.default_tp_mode) % static_cast<uint8_t>(TPMode::_TOTAL_MODES);
            } else {
                cfg.default_tp_mode = static_cast<TPMode>(menu_buf.value % static_cast<uint8_t>(TPMode::_TOTAL_MODES));
                cfg.save();
            }
            break;
        default:
            return nullptr;
    }
    return &menu_buf;
}

MD_Menu::value_t *handle_int(MD_Menu::mnuId_t id, bool get) {
    switch (id) {
        case 15:
            if (get) {
                menu_buf.value = cfg.tp_calib.zeroLevel;
            } else {
                cfg.tp_calib.zeroLevel = menu_buf.value;
                cfg.save();
            }
            break;
        case 19:
            if (get) {
                menu_buf.value = cfg.stick_hold;
            } else {
                cfg.stick_hold = menu_buf.value;
                cfg.save();
            }
            break;
        // ATRF Max Segments
        case 40:
            if (get) {
                menu_buf.value = cfg.max_segs;
            } else {
                cfg.max_segs = menu_buf.value;
                cfg.save();
            }
            break;
        // ATRF Segment Multiplier
        case 41:
            if (get) {
                menu_buf.value = cfg.seg_mult ? 2 : 1;
            } else {
                cfg.seg_mult = menu_buf.value == 2 ? true : false;
            }
            break;
        default:
            return nullptr;
    }
    return &menu_buf;
}

MD_Menu::value_t *handle_bool(MD_Menu::mnuId_t id, bool get) {
    switch (id) {
        case 16:
            if (get) {
                menu_buf.value = cfg.ds4_passthrough;
            } else {
                cfg.ds4_passthrough = menu_buf.value;
                cfg.save();
            }
            break;
        case 17:
            if (get) {
                menu_buf.value = cfg.perf_ctr;
            } else {
                cfg.perf_ctr = menu_buf.value;
                cfg.save();
            }
            break;
        default:
            return nullptr;
    }
    return &menu_buf;
}

MD_Menu::value_t *io_test_wrapper(MD_Menu::mnuId_t id, bool get) {
    if (!get) {
        switch (id) {
            case 10:
                _task = TASK_BTNTEST;
                break;
            case 11:
                _task = TASK_TPTEST;
                break;
        }
    }
    return nullptr;
}

MD_Menu::value_t *tp_calib_wrapper(MD_Menu::mnuId_t id, bool get) {
    if (!get && id == 12) {
        _task = TASK_TPCALIB;
    }
    return nullptr;
}

MD_Menu::value_t *reboot_wrapper(MD_Menu::mnuId_t id, bool get) {
    if (!get) {
        // issue reset request then halt
        switch (id) {
            case 20:
                disp_control(MD_Menu::DISP_CLEAR);
                LCD.setCursor(0, 0);
                LCD.print("Restart");
                (*(volatile uint32_t *) 0xe000ed0c) = 0x05fa0004;
                while (1);
                break;
            case 21:
                disp_control(MD_Menu::DISP_CLEAR);
                LCD.setCursor(0, 0);
                LCD.print("Jump to BL");
                _reboot_Teensyduino_();
                while (1);
                break;
        }
    }
    return nullptr;
}

MD_Menu::value_t *clear_eeprom_wrapper(MD_Menu::mnuId_t id, bool get) {
    if (!get) {
        cfg.reset();
        cfg.save();
    }
    return nullptr;
}

static void service_menu_setup(void) {
    pinMode(QEI_SW, INPUT_PULLUP);
    pinMode(BTN_CSL, OUTPUT);
    pinMode(BTN_CSB, OUTPUT);
    digitalWrite(BTN_CSL, HIGH);
    digitalWrite(BTN_CSB, HIGH);

    cfg.load();

    qei_sw_block = true;

    QEI.write(0);
    SPI.begin();

    digitalWrite(BTN_CSL, LOW);
    SPI.beginTransaction(SPI596);
    SPI.transfer(0xc0);
    digitalWrite(BTN_CSL, HIGH);
    SPI.endTransaction();

    disp_control(MD_Menu::DISP_INIT, nullptr);

    reset_qei_sw();
    
    SoftPotMagic.begin(SP_L, SP_R, respAnalogRead);
    SoftPotMagic.setCalib(&(cfg.tp_calib));
    SoftPotMagic.setMinGapRatioInt(cfg.tp_gap_ratio);

    TestMenu.begin();
    TestMenu.setMenuWrap(true);
    _task = TASK_MENU;
}

static void service_menu_loop(void) {
    // persistent tasks
    scan_qei_sw();

    // foreground task
    switch (_task) {
        case TASK_MENU:
            if (!TestMenu.isInMenu()) {
                QEI.write(0);
                TestMenu.runMenu(true);
            } else {
                TestMenu.runMenu();
            }
            break;
        case TASK_BTNTEST:
            button_test();
            break;
        case TASK_TPTEST:
            tp_test();
            break;
        case TASK_TPCALIB:
            tp_calib();
            break;
    }
}

void service_menu_main(void) {
    service_menu_setup();
    while (1) {
        service_menu_loop();
    }
}
