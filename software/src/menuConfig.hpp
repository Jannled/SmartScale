#ifndef SMARTSCALE_MENUCONFIG_H
#define SMARTSCALE_MENUCONFIG_H

#include <stddef.h>
#include <stdint.h>

extern uint8_t state;

/**
 * Print the current WiFi status to the Serial Conosle
 * @param verbose Print additional information
 */
void printWiFiStatus(bool verbose = false);

size_t serialReadLine(char *buffer, size_t length);

void updateMenu();

#endif // SMARTSCALE_MENUCONFIG_H