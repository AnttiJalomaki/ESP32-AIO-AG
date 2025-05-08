#include "ethernet.h"

#include "config/pinout.h"
#include "config/defines.h"
#include "utils/log.h"
#include "w6100/esp32_sc_w6100.h"

// Ethernet configuration
#if !USE_DHCP
IPAddress staticIP(STATIC_IP_ADDR);
IPAddress gateway(STATIC_GW_ADDR);
IPAddress subnet(STATIC_SN_ADDR);
IPAddress dns(STATIC_DNS_ADDR);
#endif

void getMacAddress(uint8_t* mac) {
    uint64_t chipmacid = ESP.getEfuseMac();
    mac[5] = (chipmacid >> 40) & 0xFF;
    mac[4] = (chipmacid >> 32) & 0xFF;
    mac[3] = (chipmacid >> 24) & 0xFF;
    mac[2] = (chipmacid >> 16) & 0xFF;
    mac[1] = (chipmacid >> 8) & 0xFF;
    mac[0] = chipmacid & 0xFF;
}

bool initializeEthernet() {
    debug("Initializing Ethernet...");
    
    // Get the ESP32's MAC address
    uint8_t mac[6];
    getMacAddress(mac);
    
    debugf("MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    ETH.begin(W6100_MISO_GPIO, W6100_MOSI_GPIO, W6100_SCK_GPIO, W6100_CS_GPIO, W6100_INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac);
    // ESP32_W6100_onEvent(); // Commented out for diagnostics
    delay(500); // Keep existing delay to allow W6100 to settle

    bool dhcp_success = false;

    if (USE_DHCP) {
        debug("Waiting for DHCP connection...");
        unsigned long startTime = millis();
        const unsigned long linkTimeoutMs = 10000; // 10 seconds for link to be up
        const unsigned long dhcpNegotiationTimeoutMs = 30000; // Specific timeout for DHCP negotiation phase

        debug("Checking Ethernet link status...");
        // Assumes ETH.linkSpeed() returns 0 or less if link is down, >0 if up.
        while (ETH.linkSpeed() <= 0) { 
            if (millis() - startTime > linkTimeoutMs) {
                error("Ethernet Link is Down. Timeout waiting for link.");
                goto end_network_config; // Skip to final check
            }
            debug("Ethernet link is not up. Retrying in 1s...");
            delay(1000);
        }
        debug("Ethernet Link is UP.");

        // Reset start time for DHCP attempt after link is confirmed up
        startTime = millis(); 
        debug("Attempting to obtain IP via DHCP...");
        while (ETH.localIP() == INADDR_NONE) {
            if (millis() - startTime > dhcpNegotiationTimeoutMs) { // Use the new specific timeout
                errorf("DHCP Timeout. Could not obtain IP address in %lu ms.", dhcpNegotiationTimeoutMs);
                goto end_network_config; // Skip to final check
            }
            delay(100); // Polling delay
        }
        dhcp_success = (ETH.localIP() != INADDR_NONE);
    } else {
#if !USE_DHCP
        debug("Configuring Static IP. Checking Ethernet link status...");
        unsigned long startTime = millis();
        const unsigned long linkTimeoutMs = 10000; // 10 seconds for link
        // Assumes ETH.linkSpeed() returns 0 or less if link is down, >0 if up.
        while (ETH.linkSpeed() <= 0) {
            if (millis() - startTime > linkTimeoutMs) {
                error("Ethernet Link is Down for Static IP. Timeout.");
                break; // Proceed to config, library might handle it or fail
            }
            debug("Ethernet link is not up (static config). Retrying in 1s...");
            delay(1000);
        }

        if(ETH.linkSpeed() > 0) {
            debug("Ethernet Link is UP for Static IP.");
        } else {
            warning("Ethernet Link still not up, proceeding with static IP config anyway.");
        }
        // These variables (staticIP, gateway, subnet, dns) are defined at file scope inside #if !USE_DHCP
        debugf("Configuring static IP: %s", staticIP.toString().c_str());
        ETH.config(staticIP, gateway, subnet, dns);
        delay(100); // Added small delay after ETH.config()
#endif
    }

end_network_config:; // Label for goto

    if (ETH.localIP() == INADDR_NONE) {
        const char* linkStatusStr = "Unknown";
        int currentSpeed = ETH.linkSpeed(); // Get speed once
        
        if (currentSpeed > 0) {
            linkStatusStr = "ON";
        } else {
            linkStatusStr = "OFF";
        }
        
        errorf("Failed to configure network. Current IP: %s, Link Speed: %d Mbps, Link Status: %s", 
               ETH.localIP().toString().c_str(), currentSpeed, linkStatusStr);
        return false;
    }

    debugf("Network configured - IP: %s", ETH.localIP().toString().c_str());
    return true;
}
