#ifndef HARDWARE_H_
#define HARDWARE_H_

#include "../autosteer/buttons.h" // For ButtonsInterface
#include "buttons/buttons.h"     // For hw::Buttons

namespace hw{

bool init();

// Define the implementation of the ButtonsInterface
static const buttons::ButtonsInterface ButtonsInterfaceImpl = {
    .isAutoSteerEnabled = hw::Buttons::isAutoSteerEnabled,
    .isSteerLeftPressed = hw::Buttons::isSteerLeftPressed,
    .isSteerRightPressed = hw::Buttons::isSteerRightPressed
};

}

#endif //HARDWARE_H_
