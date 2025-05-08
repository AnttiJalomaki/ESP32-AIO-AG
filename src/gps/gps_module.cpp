#include "gps_module.h"
#include "config/pinout.h"
#include "config/defines.h"
#include "../network/udp.h"
#include "../utils/log.h"
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

namespace gps {
constexpr size_t test_bauds_len          = 3;
constexpr int test_bauds[test_bauds_len] = {38400, 115200, 230400};
constexpr int selected_baud              = 230400;

static bool gpsConnected = false;

// Define the target IP and port for sending GPS data
static bool (*udp_send_func)(const uint8_t *, size_t) = nullptr;

// GNSS module instance
static SFE_UBLOX_GNSS myGNSS;

bool configureGPS();

// Initialize GPS module
bool init() {
    bool resp = false;
    debug("Initializing GPS...");
    for (const int test_baud: test_bauds) {
        debugf("Testing baud rate: %d", test_baud);
        GPSSerial.end();
        GPSSerial.setRxBufferSize(1024 * 5);
        GPSSerial.begin(test_baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
        delay(500);
        if ((resp = myGNSS.begin(GPSSerial, defaultMaxWait, false))) {
            break;
        }
    }

    if (!resp) {
        error("GPS - Not detected");
        gpsConnected = false;
        return false;
    }

    // Configure the GPS module
    gpsConnected = configureGPS();

    return true;
}

bool configureGPS() {
    bool resp = true;
    // update uart1 baud rate
    debugf("Setting UART1 baud rate to %d", selected_baud);
    myGNSS.setSerialRate(selected_baud, COM_PORT_UART1); // Set the UART port to fast baud rate
    GPSSerial.end();
    GPSSerial.setRxBufferSize(1024 * 5);
    GPSSerial.begin(selected_baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    resp = myGNSS.begin(GPSSerial, defaultMaxWait, false);

    if (GPS_DEFAULT_CONFIGURATION) {
        if (resp) { // Only configure if initial begin was successful
            debug("GPS - Applying default configuration...");
            if (!myGNSS.setNavigationFrequency(10)) {
                error("GPS - Failed to set navigation frequency.");
                resp = false; // Update main resp
            }

            if (resp && !myGNSS.setUART1Output(COM_TYPE_UBX | COM_TYPE_NMEA)) {
                error("GPS - Failed to set UART1 output (UBX/NMEA).");
                resp = false; // Update main resp
            }

            if (resp) { // Only enable messages if previous steps succeeded
                debug("GPS - Enabling NMEA messages...");
                bool nmea_config_ok = true; // Use a local flag for this block of settings
                nmea_config_ok &= myGNSS.enableNMEAMessage(UBX_NMEA_GGA, COM_PORT_UART1);
                nmea_config_ok &= myGNSS.enableNMEAMessage(UBX_NMEA_GSA, COM_PORT_UART1);
                nmea_config_ok &= myGNSS.enableNMEAMessage(UBX_NMEA_GSV, COM_PORT_UART1);
                nmea_config_ok &= myGNSS.enableNMEAMessage(UBX_NMEA_RMC, COM_PORT_UART1);
                nmea_config_ok &= myGNSS.enableNMEAMessage(UBX_NMEA_GST, COM_PORT_UART1); // Important for accuracy assessment
                nmea_config_ok &= myGNSS.enableNMEAMessage(UBX_NMEA_VTG, COM_PORT_UART1);
                nmea_config_ok &= myGNSS.enableNMEAMessage(UBX_NMEA_GLL, COM_PORT_UART1);

                if (!nmea_config_ok) {
                    error("GPS - Failed to enable one or more NMEA messages.");
                    resp = false; // Update main resp
                } else {
                    debug("GPS - NMEA messages enabled successfully.");
                }
            }

            if (resp) {
                debug("GPS - Default configuration applied successfully.");
            } else {
                error("GPS - Failed to apply default configuration.");
            }
        } else {
            debug("GPS - Skipping default configuration due to earlier initialization failure.");
        }
    }

    if (resp == false) {
        error("GPS - Failed to set GPS mode.");
    } else {
        debug("GPS - Module configuration complete");
    }
    return resp;
}

// Forward correction data from UDP to Serial
bool forward_udp_to_serial(const uint8_t *data, size_t len) {
    if (!gpsConnected) {
        return false;
    }
    // Forward raw data to GPS via serial
    GPSSerial.write(data, len);
    return true;
}

// Forward correction data from UDP to Serial
bool forward_correction_to_serial(const uint8_t *data, size_t len) {
    return forward_udp_to_serial(data, len);
}

// Set the UDP sender function for GPS data
void set_udp_sender(bool (*send_func)(const uint8_t *, size_t)) {
    udp_send_func = send_func;
}

// Handle UDP messages received from AgOpenGPS
void process_udp_message(const uint8_t *data, size_t len, const ip_address &sourceIP) {
    // Forward the message to the GPS
    forward_udp_to_serial(data, len);
}

const size_t buffer_size = 256; // Set the buffer size for NMEA messages
static uint8_t buffer[buffer_size];
int buffer_pos = 0; // Initialize buffer position

void handler() {
    if (!gpsConnected || udp_send_func == nullptr) {
        return;
    }

    while (GPSSerial.available()) {
        char c = GPSSerial.read();

        if (buffer_pos < buffer_size) { // Check if buffer has space
            buffer[buffer_pos++] = (uint8_t)c;
        } else {
            // Buffer is full, but no newline yet. Send what we have to avoid data loss.
            // This case should ideally not be hit if NMEA sentences are shorter than buffer_size.
            warnf("GPS handler: buffer full before newline, sending partial data. Size: %d", buffer_pos);
            udp_send_func(buffer, buffer_pos);
            buffer_pos = 0;       // Reset buffer
            buffer[buffer_pos++] = (uint8_t)c; // Store current char in new buffer
        }

        if (c == '\\n') {
            // End of an NMEA sentence (typically preceded by '\\r')
            // Send the complete sentence
            udp_send_func(buffer, buffer_pos);
            buffer_pos = 0; // Reset buffer for the next sentence
        }
    }
    // Note: If GPSSerial.available() is false and buffer_pos > 0 but no '\\n' was received,
    // that partial data remains in the buffer until the next call to handler() 
    // and more data arrives to complete the sentence or fill the buffer.
}
} // namespace gps

// Initialize GPS communication with UDP sending function and device IP
void initGpsCommunication(bool (*send_func)(const uint8_t *, size_t), const ip_address &deviceIP) {
    gps::set_udp_sender(send_func);
}
