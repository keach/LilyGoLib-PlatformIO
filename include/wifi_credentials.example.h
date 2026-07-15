#pragma once

#include "wifi_credentials_types.h"

// Copy this file to wifi_credentials.h and list each trusted network.
// wifi_credentials.h is intentionally excluded from Git.
inline constexpr WiFiCredential kWiFiCredentials[] = {
    {"HOME_WIFI", "home-password"},
    {"MOBILE_HOTSPOT", "hotspot-password"},
};

inline constexpr size_t kWiFiCredentialCount =
    sizeof(kWiFiCredentials) / sizeof(kWiFiCredentials[0]);
