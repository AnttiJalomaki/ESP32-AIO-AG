#include "buttons.h"
#include <Arduino.h>
#include "../../autosteer/buttons.h"

namespace hw {

bool Buttons::initialized = false;

bool Buttons::init() {
    if (initialized) {
        return true;
    }

    pinMode(BUTTON_AUTOSTEER_PIN, INPUT_PULLUP);
    pinMode(BUTTON_STEER_LEFT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_STEER_RIGHT_PIN, INPUT_PULLUP);

    initialized = true;
    return initialized;
}

bool Buttons::isAutoSteerEnabled() {
    if (!initialized) {
        init();
    }
    return digitalRead(BUTTON_AUTOSTEER_PIN) == LOW;
}

bool Buttons::isSteerLeftPressed() {
    if (!initialized) {
        init();
    }
    return digitalRead(BUTTON_STEER_LEFT_PIN) == LOW;
}

bool Buttons::isSteerRightPressed() {
    if (!initialized) {
        init();
    }
    return digitalRead(BUTTON_STEER_RIGHT_PIN) == LOW;
}

} // namespace hw