#pragma once
#include <DNSServer.h>
#include <WebServer.h>

#include "app/WiFiApp/BeaconSpam.h"
#include "app/WiFiApp/EchoSniffer.h"
#include "app/WiFiApp/WiFiConnect.h"
#include "app/WiFiApp/WiFiHotspot.h"
#include "app/WiFiApp/WiFiScanner.h"
#include "app/WiFiApp/WiFiSniffer.h"
#include "app/WiFiApp/WiFiStorm.h"
#include "app/WiFiApp/WiFiTelnet.h"

extern DNSServer dnsServer;
extern WebServer server;
// True while the hotspot's captive-portal DNS is actually listening.
// dnsServer has no state query of its own, and parsing packets from a
// socket that was never opened logs "could not receive data: 9" (EBADF)
// on every main-loop iteration.
extern bool dnsRunning;