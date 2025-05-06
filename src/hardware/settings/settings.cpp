#include "settings.h"
#include <Arduino.h>
#include <EEPROM.h>
#include "../../autosteer/settings.h"

namespace hw {

bool Settings::initialized = false;

bool Settings::init() {
    if (initialized) {
        return true;
    }
    EEPROM.begin(512); // Example size, adjust as needed
    initialized = true;
    return initialized;
}

uint8_t Settings::read(uint8_t address) {
    if (!initialized) {
        init();
    }
    return EEPROM.read(address);
}

void Settings::write(uint8_t address, uint8_t value) {
    if (!initialized) {
        init();
    }
    EEPROM.write(address, value);
    EEPROM.commit(); // Or EEPROM.end() depending on library version and usage
}

} // namespace hw