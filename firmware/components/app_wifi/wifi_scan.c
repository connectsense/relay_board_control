/*
 * wifi_scan.c
 *
 *  Created on: Oct 14, 2022
 *      Author: wesd
 */
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>

#include "wifi_scan.h"

static const char	*TAG = "wifi_scan";

esp_err_t wifiApScan(wifi_ap_record_t **apRecs, uint16_t *apCount)
{
	esp_err_t	status;

	*apRecs  = NULL;
	*apCount = 0;

	wifi_scan_config_t	scanCfg = {
		.ssid                 = NULL,
		.bssid                = NULL,
		.channel              = 0,
		.show_hidden          = true,
		.scan_type            = WIFI_SCAN_TYPE_ACTIVE,
//		.scan_time.active.min = 200,
//		.scan_time.active.max = 1000,
	};

	int			tries;
	uint16_t	numRecs;

	for (tries = 0; tries < 4; tries++) {
		// Run the AP scan, wait for it to complete
		status = esp_wifi_scan_start(&scanCfg, true);
		if (ESP_OK != status) {
			ESP_LOGE(TAG, "esp_wifi_scan_start error %X", status);
			return status;
		}
		(void)esp_wifi_scan_stop();

		// Retrieve the number of APs found
		esp_wifi_scan_get_ap_num(&numRecs);

		if (numRecs > 0)
			break;

		// No APs founds, try again in 250 ms
		vTaskDelay(pdMS_TO_TICKS(250));
	}

	if (numRecs > 0) {
		wifi_ap_record_t *	apList;

		// Allocate space to hold the wifi AP list array
		apList = calloc(numRecs, sizeof(wifi_ap_record_t));
		if (apList) {
			// Read the AP list
			status = esp_wifi_scan_get_ap_records(&numRecs, apList);
			if (ESP_OK == status) {
				// Pass back the results
				*apRecs  = apList;
				*apCount = numRecs;
			} else {
				ESP_LOGE(TAG, "esp_wifi_scan_get_ap_records error %X", status);
				free(apList);
			}
		} else {
			ESP_LOGE(TAG, "Failed to allocate memory for AP list");
			status = ESP_ERR_NO_MEM;
		}
	}

	return status;
}


void wifiApRelease(wifi_ap_record_t *apRecs)
{
	if (apRecs) {
		free(apRecs);
	}
}


void wifiApSort(wifi_ap_record_t *pList, uint16_t *pLength)
{
	wifi_ap_record_t	swap;
	wifi_ap_record_t *	pSlot;
	wifi_ap_record_t *	pTest;
	wifi_ap_record_t *	pHigh;
	unsigned int		i1;
	unsigned int		i2;

	// Sort by ascending SSID
	pSlot = pList;
	for (i1 = 0; i1 < *pLength - 1; i1++, pSlot++) {
		pHigh = pSlot;
		pTest = pSlot + 1;

		for (i2 = 0; i2 < *pLength - i1 - 1; i2++, pTest++) {
			if (strcmp((char *)pTest->ssid, (char *)pHigh->ssid) < 0) {
				pHigh = pTest;
			}
		}

		if (pHigh != pSlot) {
			swap   = *pSlot;
			*pSlot = *pHigh;
			*pHigh = swap;
		}
	}

	// Next sort matching SSIDs by descending RSSI
	pSlot = pList;
	for (i1 = 0; i1 < *pLength - 1; i1++, pSlot++) {
		pHigh = pSlot;
		pTest = pSlot + 1;

		for (i2 = 0; i2 < *pLength - i1 - 1; i2++, pTest++) {
			int	ssid1Len = strlen((char *)pSlot->ssid);
			int	ssid2Len = strlen((char *)pTest->ssid);

			if (ssid1Len != ssid2Len) {
				break;
			}
			if (memcmp(pSlot->ssid, pTest->ssid, ssid1Len) != 0) {
				break;
			}

			if (pTest->rssi > pHigh->rssi) {
				pHigh = pTest;
			}
		}

		if (pHigh != pSlot) {
			swap   = *pSlot;
			*pSlot = *pHigh;
			*pHigh = swap;
		}
	}

	// Now step through the list, removing duplicate SSIDs
	pSlot = pList;
	for (i1 = 0; i1 < *pLength - 1; i1++, pSlot++) {
		pTest = pSlot + 1;
		for (i2 = i1 + 1; i2 < *pLength; ) {
			int	ssid1Len = strlen((char *)pSlot->ssid);
			int	ssid2Len = strlen((char *)pTest->ssid);
			int	shiftCt;


			if (ssid1Len != ssid2Len) {
				break;
			}
			if (memcmp(pSlot->ssid, pTest->ssid, ssid1Len) != 0) {
				break;
			}

			shiftCt = (*pLength - i2 - 1);
			if (shiftCt) {
				memcpy(pTest, pTest + 1, shiftCt * sizeof(wifi_ap_record_t));
			}

			*pLength -= 1;
		}
	}
}
