#ifndef SMARTSCALE_MENUCONFIG_H
#define SMARTSCALE_MENUCONFIG_H

#include <stddef.h>
#include <stdint.h>

/**
 * Print the current WiFi status to the Serial Conosle
 * @param verbose Print additional information
 */
void printWiFiStatus(bool verbose = false);

size_t serialReadLine(char *buffer, size_t length);

void initMenu();

void updateMenu();

/**
 * @brief Print a text line that explains how the user can open the menu
 * 
 */
void printMenuInfo();

/**
 * @brief Silent operation
 * 
 * @param currState 0
 * @return uint16_t 
 */
uint16_t waitForInput(uint16_t currState);

/**
 * @brief Close the menu and print the reopen message.
 * 
 * @param currState 1
 * @return uint16_t 0
 */
uint16_t closeMenu(uint16_t currState);

/**
 * @brief Normal operation
 * 
 * @param currState 2
 * @return uint16_t 
 */
uint16_t printCurrentWeight(uint16_t currState);

/**
 * @brief SmartScale Menu
 * 
 * @param currState 3
 * @return uint16_t 
 */
uint16_t printMenu(uint16_t currState);

/**
 * @brief Wait for the user to pick one of the menue entrys.
 * 
 * @param currState 4
 * @return uint16_t The new state according to the selected menu entry.
 */
uint16_t menuWaitForInput(uint16_t currState);

/**
 * @brief 2. Calibrate Scale
 * 
 * @param currState 5
 * @return uint16_t 
 */
uint16_t calibrateScale(uint16_t currState);

/**
 * @brief 3. Change Wifi SSID and Password
 * 
 * @param currState 6
 * @return uint16_t 
 */
uint16_t changeWifiSettings(uint16_t currState);

/**
 * @brief 4. Print WiFi status
 * 
 * @param currState 7
 * @return uint16_t 
 */
uint16_t printWifiInfo(uint16_t currState);

/**
 * @brief 5. List all WiFi Stations
 * 
 * @param currState 8
 * @return uint16_t 
 */
uint16_t printWifiNetworks(uint16_t currState);

/**
 * @brief 6. Turn off HX711
 * 
 * @param currState 9
 * @return uint16_t 
 */
uint16_t disableHX711(uint16_t currState);

#endif // SMARTSCALE_MENUCONFIG_H