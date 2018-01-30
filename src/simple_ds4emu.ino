// TODO use own implementation with auth
//#include "PS4USBAuth.h"
#include <PS4USB.h>

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

USB USBH;
PS4USB RealDS4(&USBH);

bool ds4_cb_on_set_challenge(const ds4_auth_t *challenge) {
    char hexbuf[7];
    uint8_t retry, result;
    if (RealDS4.connected()) {
        for (retry=0; retry<10; retry++) {
            result = RealDS4.SetReport(0, 0, 0x03, 0xf0, sizeof(ds4_auth_t), (uint8_t *) challenge);
            if (result) {
                Serial1.print("E: set_challenge: usbh err ");
                sprintf(hexbuf, "0x%02x", result);
                Serial1.print(hexbuf);
                Serial1.println();
                continue;
            } else {
                retry = 0;
                break;
            }
        }
        if (retry) return 1;
        Serial1.print("I: seq=");
        Serial1.print(challenge->seq, DEC);
        Serial1.print(",page=");
        Serial1.print(challenge->page, DEC);
        Serial1.println();
    } else {
        Serial1.println("E: ds4 not present");
        return false;
    }
    return true;
}

//bool ds4_cb_on_challenge_response_available(ds4_auth_result_t *result) {
//    memset(result, 0, sizeof(ds4_auth_result_t));
//    result->type = 0xf2;
//    return true;
//}

bool ds4_cb_on_challenge_response_available(ds4_auth_result_t *auth_result) {
    char hexbuf[7];
    uint8_t retry, result;
    if (RealDS4.connected()) {
        for (retry=0; retry<10; retry++) {
            result = RealDS4.GetReport(0, 0, 0x03, 0xf2, sizeof(ds4_auth_result_t), (uint8_t *) auth_result);
            if (result) {
                Serial1.print("E: challenge_response_available: usbh err ");
                sprintf(hexbuf, "0x%02x", result);
                Serial1.print(hexbuf);
                Serial1.println();
                continue;
            } else {
                retry = 0;
                break;
            }
        }
        if (retry) return 1;
        Serial1.print("I: seq=");
        Serial1.print(auth_result->seq, DEC);
        Serial1.print(",ok=");
        Serial1.print((~(auth_result->status >> 4) & 1), DEC);
        Serial1.println();
    } else {
        Serial1.println("E: ds4 not present");
        return false;
    }
    return true;
}

//bool ds4_cb_on_get_challenge_response(ds4_auth_t *response) {
//    memset(response, 0, sizeof(ds4_auth_t));
//    response->type = 0xf1;
//    return true;
//}

bool ds4_cb_on_get_challenge_response(ds4_auth_t *response) {
    char hexbuf[7];
    uint8_t retry, result;
    if (RealDS4.connected()) {
        for (retry=0; retry<10; retry++) {
        result = RealDS4.GetReport(0, 0, 0x03, 0xf1, sizeof(ds4_auth_t), (uint8_t *) response);
            if (result) {
                Serial1.print("E: get_challenge_response: usbh err ");
                sprintf(hexbuf, "0x%02x", result);
                Serial1.print(hexbuf);
                Serial1.println();
                continue;
            } else {
                retry = 0;
                break;
            }
        }
        if (retry) return 1;
        Serial1.print("I: seq=");
        Serial1.print(response->seq, DEC);
        Serial1.print(",page=");
        Serial1.print(response->page, DEC);
        Serial1.println();
    } else {
        Serial1.println("E: ds4 not present");
        return false;
    }
    return true;
}

static const ds4_auth_cbt_t cbt = {
    ds4_cb_on_set_challenge,
    ds4_cb_on_challenge_response_available,
    ds4_cb_on_get_challenge_response
};

void setup() {
    Serial1.begin(115200);
    Serial1.println("I: Hello");

    Serial1.println("I: init usb");
    usb_ds4_set_cbt(&cbt);
    DS4.begin();

    Serial1.println("I: init usbh");
    if (USBH.Init() == -1) {
        Serial1.println("F: timeout waiting for usbh");
        while (1);
    }

    //pinMode(13, OUTPUT);
    //digitalWrite(13, HIGH);
}

void loop() {
    // Poll the USBH controller
    USBH.Task();
    DS4.update();
    //digitalWrite(13, LOW);
    //if (RealDS4.connected()) {
    //    Serial1.println("I: ds4 ok");
    //}
    if ((millis() / 500) & 1) {
        // Note that you need to press the PS button once to let PS4 focus on
        // your controller
        // Since this is just a test, just spam the PS button so that we know
        // the emulation works
        DS4.pressButton(DS4_BTN_PS);
    } else {
        DS4.releaseAllButton();
    }
    //if (Serial1.available() > 0) {
    //    uint8_t key = Serial1.read();
    //    Serial1.println(key);
    //    if (key == 'a') {
    //        DS4.pressDpad(DS4_DPAD_E);
    //    }
    //} else {
    //    DS4.releaseDpad();
    //}
    
    DS4.send();
    //delay(5);
    //digitalWrite(13, HIGH);
    //delay(1000);
}
