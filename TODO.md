# ESP32 Autosteer Project TODO List

This document outlines potential areas for improvement and further development for the ESP32-based autosteering project. The focus is on enhancing robustness, safety, performance, and maintainability.

## 1. Robust Error Handling & Recovery

Reliable error handling is paramount in an autosteering system to ensure safety and operational stability.

### 1.1. Module Initialization Checks

*   **Current Status:** Modules like GPS and IMU have basic initialization.
*   **Suggestion:** Implement comprehensive initialization checks for all critical hardware modules:
    *   IMU (`src/hardware/imu/bno08x_imu.cpp`)
    *   GPS (`src/gps/gps_module.cpp`)
    *   Motor Controller (`src/hardware/motor/pwm_motor.cpp`)
    *   Wheel Angle Sensor (WAS) (`src/hardware/was/ads1115_was.cpp`)
*   **Details:**
    *   Verify return codes from `begin()` or similar initialization functions.
    *   Check for expected device IDs or communication responses.
    *   If initialization fails, the system should:
        *   Enter a defined safe state (e.g., disengage autosteer, halt motor).
        *   Provide clear diagnostic information via serial log (`src/utils/log.h`) and/or status LEDs.
        *   Consider a retry mechanism for transient failures, but with a limit.
*   **Background:** Failure to initialize a sensor or actuator can lead to unpredictable behavior. A robust startup sequence ensures all components are ready before operation.

### 1.2. Runtime Error Handling

*   **Suggestion:** Enhance runtime error detection and handling.
*   **Details:**
    *   **I2C Communication (`src/hardware/i2c_manager.cpp`):** Check return values from `Wire.endTransmission()`, `Wire.requestFrom()`, etc. Handle NACKs, bus errors, or timeouts. The existing `I2C_MUTEX_LOCK/UNLOCK` is good for preventing concurrent access, but individual transaction success should also be checked.
    *   **ADC Readings (e.g., for WAS):** Validate ADC readings against expected ranges. Detect sensor disconnections or failures.
    *   **Strategy:** Define a clear strategy for handling runtime errors:
        *   **Retry:** For transient issues, a few retries might be acceptable.
        *   **Re-initialize:** Attempt to re-initialize the problematic module.
        *   **Safe Mode:** If critical errors persist, transition the system to a safe operational mode (e.g., manual steering only).
        *   **Logging:** Always log errors with sufficient detail for debugging.
*   **Background:** Runtime errors can occur due to loose connections, power fluctuations, or component failures. The system must react gracefully.

### 1.3. Watchdog Timers (WDT)

*   **Suggestion:** Implement both hardware (HW WDT) and software (task-level WDT) watchdogs.
*   **Details:**
    *   **Hardware WDT:** The ESP32 has a built-in HW WDT. Configure it to reset the MCU if the main loop or critical tasks hang. Ensure it's fed (reset) regularly from a known-good part of your main control loop.
    *   **Software WDT (Task Watchdog):** For critical FreeRTOS tasks (e.g., autosteer logic, motor control), implement a task-specific watchdog. Each monitored task would periodically signal its aliveness. A supervisor task would check these signals and take action (e.g., reset, log error, enter safe mode) if a task becomes unresponsive.
*   **Background:** Watchdogs are a last line of defense against software freezes or infinite loops, which could be dangerous in an autosteering application.

## 2. Task Management and Concurrency (FreeRTOS)

Proper use of FreeRTOS features is crucial for a responsive and stable multi-tasking system.

### 2.1. Task Priorities

*   **Current Status:** Task priorities are defined in `src/config/defines.h`.
*   **Suggestion:** Review and fine-tune task priorities.
*   **Details:**
    *   Safety-critical tasks (e.g., motor control (`src/autosteer/motor.cpp`), main steer logic (`src/autosteer/steer_logic.h`, `src/autosteer/autosteer.cpp`)) should have higher priorities than less critical tasks (e.g., logging, UI updates if any).
    *   Ensure that high-frequency tasks (like IMU reading or PID calculation) can meet their deadlines.
    *   Avoid priority inversion: If a high-priority task waits for a resource held by a lower-priority task, use mutexes with priority inheritance.
*   **Background:** Incorrect task prioritization can lead to missed deadlines, jitter, or even starvation of critical tasks.

### 2.2. Stack Sizes

*   **Suggestion:** Profile or estimate stack usage for each FreeRTOS task.
*   **Details:**
    *   Use tools like `uxTaskGetStackHighWaterMark()` after tasks have been running under load to determine the minimum free stack space.
    *   Set stack sizes in task creation calls with some margin to prevent overflows.
    *   PlatformIO's debugger or ESP-IDF tools can assist in stack analysis.
*   **Background:** Stack overflows can corrupt data and lead to crashes that are hard to debug.

### 2.3. Inter-Task Communication & Synchronization

*   **Current Status:** `I2C_MUTEX_LOCK/UNLOCK` is used.
*   **Suggestion:** Ensure all shared resources and inter-task data exchanges are thread-safe.
*   **Details:**
    *   **Mutexes:** For protecting shared resources (e.g., global variables, hardware peripherals accessed by multiple tasks). Ensure mutexes are always released.
    *   **Semaphores:** For signaling and synchronization (e.g., a task waiting for data to be ready).
    *   **Queues:** For passing data between tasks (e.g., GPS data from a GPS handler task to the main autosteer task). This is generally safer and more robust than shared global variables.
    *   Avoid busy-waiting or polling. Use FreeRTOS blocking calls with timeouts where appropriate.
*   **Background:** Race conditions and data corruption are common issues in concurrent systems if shared resources are not properly protected.

### 2.4. CPU Core Affinity

*   **Suggestion:** For computationally intensive or time-sensitive tasks, consider pinning them to a specific CPU core on the dual-core ESP32.
*   **Details:**
    *   Use `xTaskCreatePinnedToCore()`.
    *   For example, the main autosteer calculations or fast sensor processing might benefit from being on a dedicated core, separate from Wi-Fi/Bluetooth stacks (which often run on a specific core by default).
*   **Background:** Core pinning can reduce jitter and improve determinism for critical tasks by avoiding task migration between cores.

## 3. Autosteer Logic (`src/autosteer/`)

Refinements to the core autosteering algorithms and state management can improve performance and safety.

### 3.1. State Machine

*   **Suggestion:** Implement or formalize a clear state machine for the autosteering system.
*   **Details:**
    *   Define states like: `IDLE`, `ARMED` (ready to engage), `ENGAGED`, `PAUSED`, `DISENGAGING`, `ERROR`.
    *   Clearly define transitions between states and the conditions that trigger them.
    *   This makes the system's behavior more predictable and easier to debug.
*   **Background:** A well-defined state machine helps manage complexity and ensures the system behaves consistently.

### 3.2. Pre-Engagement Safety Checks

*   **Suggestion:** Implement comprehensive safety checks before allowing autosteer engagement.
*   **Details:**
    *   GPS fix quality (e.g., RTK fixed, sufficient satellites, low HDOP).
    *   IMU health (e.g., data is recent, values are within expected ranges).
    *   WAS calibration status and health.
    *   Vehicle speed within acceptable limits for engagement.
    *   No critical system errors active.
*   **Background:** Prevents engagement in unsafe conditions, protecting equipment and personnel.

### 3.3. PID Controller (`src/autosteer/pid_controller.cpp`)

*   **Suggestion:** Enhance PID controller functionality and tuning.
*   **Details:**
    *   **Tuning:** PID parameters (Kp, Ki, Kd) are critical. Provide a mechanism for easy tuning (e.g., via serial commands, UDP messages from AgOpenGPS, or a simple web interface).
    *   **Logging:** Log PID inputs (setpoint, actual value, error), individual terms (P, I, D), and the final output. This is invaluable for tuning and diagnosing issues.
    *   **Anti-Windup:** Ensure proper integral anti-windup is implemented to prevent the integral term from accumulating excessively when the output is saturated.
    *   **Adaptive Tuning:** Consider if PID parameters might need to adapt based on speed or other conditions (more advanced).
*   **Background:** A well-tuned PID controller is essential for accurate and stable steering.

### 3.4. Failsafe Mechanisms

*   **Suggestion:** Implement robust failsafe mechanisms.
*   **Details:**
    *   **GPS Data Loss/Degradation:** If GPS fix quality degrades significantly or data is lost while engaged, the system should disengage safely (e.g., ramp down steering commands).
    *   **IMU Data Loss/Failure:** Similar to GPS, if IMU data becomes unreliable or stops, disengage.
    *   **WAS Failure:** If the wheel angle sensor readings become erratic or fail, disengage.
    *   **Communication Loss with AgOpenGPS:** If commands or heartbeats from AgOpenGPS stop, the system should disengage after a timeout.
*   **Background:** Ensures the system reverts to a safe state if critical sensor data or communication is lost.

## 4. Configuration and Calibration (`src/autosteer/settings.cpp`)

Managing settings and calibration data reliably is important for consistent operation.

### 4.1. Persistent Storage (NVS/SPIFFS/LittleFS)

*   **Current Status:** Settings are likely stored, possibly in NVS (Non-Volatile Storage).
*   **Suggestion:** Review and ensure best practices for persistent storage.
*   **Details:**
    *   **Minimize Writes:** NVS and flash file systems have a limited number of write cycles. Minimize unnecessary writes of settings.
    *   **Atomic Operations:** If possible, update settings in a way that is atomic or use a commit mechanism to prevent data corruption if a reset occurs during a write.
    *   **Error Handling:** Check return codes from NVS or file system operations.
*   **Background:** Proper flash management prolongs the life of the ESP32's flash memory.

### 4.2. Checksums and Validation

*   **Suggestion:** When loading settings or calibration data from persistent storage, validate them.
*   **Details:**
    *   Store a checksum (e.g., CRC32) along with the settings.
    *   Verify the checksum on load.
    *   Validate individual settings against plausible ranges.
    *   If corruption is detected or settings are invalid, revert to default (safe) settings and notify the user.
*   **Background:** Protects against using corrupted settings that could lead to malfunction.

## 5. Networking (`src/network/`)

Reliable communication with AgOpenGPS is key.

### 5.1. UDP Reliability

*   **Current Status:** UDP is used for communication.
*   **Suggestion:** Consider strategies if UDP packet loss becomes an issue.
*   **Details:**
    *   **Sequence Numbers:** Add sequence numbers to UDP packets sent from the ESP32. AgOpenGPS could then detect lost packets.
    *   **Redundancy/Frequency:** For critical data, consider sending it more frequently or with some form of simple redundancy if bandwidth allows and the application can handle duplicates.
    *   **NMEA Buffering (`src/gps/gps_module.cpp`):** The recent change to buffer NMEA sentences until a newline is good. Ensure the buffer size (`buffer_size = 256`) is adequate for the longest expected NMEA sentences to avoid fragmentation or data loss if `warnf` for buffer full is hit often.
*   **Background:** UDP is connectionless and doesn't guarantee delivery. While often fine for local networks, packet loss can occur.

### 5.2. Connection Management

*   **Suggestion:** Implement clear logic for managing the "connection" state with AgOpenGPS.
*   **Details:**
    *   **Heartbeats:** The ESP32 could send periodic heartbeat messages to AgOpenGPS. AgOpenGPS could also send heartbeats.
    *   **Timeout:** If heartbeats are not received for a certain period, assume disconnection and take appropriate action (e.g., disengage autosteer).
    *   **Reconnection Logic:** Define how the system behaves when a connection is lost and then re-established.
*   **Background:** Provides a more robust link between the ESP32 and the control software.

## 6. Code Structure and Maintainability

Good code practices make the project easier to understand, debug, and extend.

### 6.1. Modularity

*   **Current Status:** The project has a good modular structure (e.g., `autosteer/`, `gps/`, `hardware/`).
*   **Suggestion:** Continue to enforce strong modularity and loose coupling between components.
*   **Background:** Makes the system easier to develop, test, and maintain.

### 6.2. Constants vs. Magic Numbers

*   **Suggestion:** Scour the codebase for any "magic numbers" (unnamed numerical constants) and replace them with named constants (e.g., `#define` or `static constexpr`) in relevant headers (like `src/config/defines.h`) or at the top of `.cpp` files.
*   **Details:** For example, thresholds, pin numbers (already good in `pinout.h`), default values, conversion factors.
*   **Background:** Improves code readability and makes it easier to modify parameters.

### 6.3. Logging (`src/utils/log.h`)

*   **Current Status:** A logging utility exists.
*   **Suggestion:** Ensure consistent and appropriate use of logging levels.
*   **Details:**
    *   `debug()`: For detailed information useful during development.
    *   `info()`: For general operational messages.
    *   `warn()`: For potential issues that don't immediately halt operation.
    *   `error()`: For critical errors that affect functionality.
    *   Avoid excessive logging in performance-critical, high-frequency loops as it can impact timing. Consider conditional compilation for verbose debug logs.
*   **Background:** Good logging is essential for diagnostics and understanding system behavior.

## 7. Hardware Abstraction (`src/hardware/`)

*   **Current Status:** The use of `_hw.cpp` files (e.g., `ads1115_was.cpp` implementing a generic WAS interface) is a good pattern.
*   **Suggestion:** Maintain this abstraction.
*   **Details:** This allows for easier swapping of hardware components in the future (e.g., a different IMU or WAS type) without major changes to the core application logic.
*   **Background:** Promotes flexibility and easier hardware evolution.

This list is comprehensive but should be approached iteratively. Prioritize changes based on their impact on safety, stability, and core functionality.
