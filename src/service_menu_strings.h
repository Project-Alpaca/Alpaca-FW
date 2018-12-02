#pragma once

#define LANG_EN 0
#define LANG_JA 1

#ifndef SERVICE_MENU_LANG
#define SERVICE_MENU_LANG LANG_EN
#endif

#if SERVICE_MENU_LANG == LANG_EN
#define STR_CFM_PRESS_SEL "Press SEL"
#define STR_CFM_MODE "Mode"
#define STR_CFM_THRES "Thres."
#define STR_CFM_YN "Y/N"
#define STR_CFM_CLR_EEPROM "Clear EEPROM?"
#define STR_CFM_HOLD_FRAME "Hold Frame"
#define STR_CFM_RESTART "Restart?"
#define STR_CFM_REBOOT_TO_BL "Reboot to BL?"
#define STR_CFM_ID "ID"
#define STR_CFM_BTN "Btn."

#define STR_HDR_SERVICE_MENU "Service Menu"
#define STR_HDR_REBOOT "Reboot"
#define STR_HDR_BUTTON_CONFIG "Button Config"
#define STR_HDR_TP_SYS_CFG "TP Sys. Cfg"

#define STR_ITM_BTN_TEST "Button Test"
#define STR_ITM_TP_TEST "TP Test"
#define STR_ITM_TP_MODE "TP Mode"
#define STR_ITM_BTN_CONF "Button Conf..."
#define STR_ITM_TP_SYS_CFG "TP Sys. Cfg..."
#define STR_ITM_HOLD_FRAME "TP+A Hold Frm."
#define STR_ITM_DS4_REDIR "DS4 Redir."
#define STR_ITM_PERF_CTR "Show PerfCtr."
#define STR_ITM_CLR_EEPROM "Clear EEPROM"
#define STR_ITM_REBOOT "Reboot..."

#define STR_ITM_REBOOT_MAIN "Main System"
#define STR_ITM_REBOOT_BL "Bootloader"

#define STR_ITM_BTNCFG_ID "ID"
#define STR_ITM_BTNCFG_ASSIGN "Assignment"

#ifdef TP_RESISTIVE
#define STR_ITM_TPSYS_CALIB "TP Calibration"
#define STR_ITM_TPSYS_CALIB_ADC "TP ADC Zero"
#endif

#elif SERVICE_MENU_LANG == LANG_JA // SERVICE_MENU_LANG == "en"

#define STR_CFM_PRESS_SEL "Press SEL"
#define STR_CFM_MODE "Mode"
#define STR_CFM_THRES "Thres."
#define STR_CFM_YN "Y/N"
#define STR_CFM_CLR_EEPROM "Clear EEPROM?"
#define STR_CFM_HOLD_FRAME "Hold Frame"
#define STR_CFM_RESTART "Restart?"
#define STR_CFM_REBOOT_TO_BL "Reboot to BL?"
#define STR_CFM_ID "ID"
#define STR_CFM_BTN "Btn."

#define STR_HDR_SERVICE_MENU "\xbb\xcb\xde\xbd \xd3\xb0\xc4\xde"
#define STR_HDR_REBOOT "\xd8\xcc\xde\xb0\xc4"
#define STR_HDR_BUTTON_CONFIG "\xce\xde\xc0\xdd \xca\xb2\xc1"
#define STR_HDR_TP_SYS_CFG "TP \xbe\xaf\xc3\xb2"

#define STR_ITM_BTN_TEST "\xce\xde\xc0\xdd \xc3\xbd\xc4"
#define STR_ITM_TP_TEST "TP \xc3\xbd\xc4"
#define STR_ITM_TP_MODE "TP \xd3\xb0\xc4\xde"
#define STR_ITM_BTN_CONF "\xce\xde\xc0\xdd \xca\xb2\xc1..."
#define STR_ITM_TP_SYS_CFG "TP \xbe\xaf\xc3\xb2..."
#define STR_ITM_HOLD_FRAME "TP+A Hold Frm."
#define STR_ITM_DS4_REDIR "DS4 Redir."
#define STR_ITM_PERF_CTR "Show PerfCtr."
#define STR_ITM_CLR_EEPROM "Clear EEPROM"
#define STR_ITM_REBOOT "\xd8\xcc\xde\xb0\xc4..."

#define STR_ITM_REBOOT_MAIN "\xba\xdd\xc4\xdb\xb0\xd7\x20\xd3\xb0\xc4\xde"
#define STR_ITM_REBOOT_BL "\xcc\xde\xb0\xc4\xdb\xb0\xc0\xde"

#define STR_ITM_BTNCFG_ID "ID"
#define STR_ITM_BTNCFG_ASSIGN "\xce\xde\xc0\xdd"

#ifdef TP_RESISTIVE
#define STR_ITM_TPSYS_CALIB "TP Calibration"
#define STR_ITM_TPSYS_CALIB_ADC "TP ADC Zero"
#endif

#endif // SERVICE_MENU_LANG == "ja"
