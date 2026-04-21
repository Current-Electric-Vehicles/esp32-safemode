#pragma once

#include "esp_err.h"

namespace safemode
{

/// Start the captive portal DNS server. All A-record queries resolve to 4.3.2.1.
esp_err_t dnsServerStart();

/// Stop the DNS server and clean up.
void dnsServerStop();

}  // namespace safemode
