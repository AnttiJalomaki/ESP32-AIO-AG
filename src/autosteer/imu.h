#ifndef IMU_H
#define IMU_H

#include <stdint.h>

namespace imu {
    // Function pointer types for hardware implementation
    using ReadFunc = float (*)();

    // Interface structure for hardware implementation
    struct IMUInterface {
        ReadFunc heading = nullptr;
        ReadFunc roll = nullptr;
        ReadFunc pitch = nullptr;
    };

    // Function declarations
    bool init(const IMUInterface hw);

    float get_heading();
    float get_roll();
    float get_pitch();
}

#endif //IMU_H
