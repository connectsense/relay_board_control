/*
 * wifi_scan.h
 *
 *  Created on: Oct 14, 2022
 *      Author: wesd
 */

#ifndef MAIN_INCLUDE_WIFI_SCAN_H_
#define MAIN_INCLUDE_WIFI_SCAN_H_

#include <esp_wifi.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifiApScan(wifi_ap_record_t **apRecs, uint16_t *apCount);

void wifiApRelease(wifi_ap_record_t *apRecs);

void wifiApSort(wifi_ap_record_t *pList, uint16_t *pLength);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_INCLUDE_WIFI_SCAN_H_ */
