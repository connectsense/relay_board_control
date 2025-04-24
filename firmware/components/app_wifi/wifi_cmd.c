/*
 * wifi_cmd.c
 *
 *  Created on: Feb 15, 2023
 *      Author: wesd
 */
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "wifi_ctrl.h"
#include "wifi_scan.h"
#include "cmd_proc.h"

typedef struct {
	bool	isInitialized;
	// ToDo
} ctrl_t;

static ctrl_t *ctrl;

static bool _enterApi(void *cbData, cmdReturn_t *ret, ctrl_t **ppCtrl)
{
	*ppCtrl = cbData;
	if (!*ppCtrl || !(*ppCtrl)->isInitialized) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "wifi API not initialized";
		return false;
	}
	return true;
}

static void _scan(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	wifi_ap_record_t	*apList;
	uint16_t			apCount;

	if (wifiApScan(&apList, &apCount) != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Wi-Fi scan failed";
		return;
	}

	wifiApSort(apList, &apCount);

	ret->jResult = cJSON_CreateArray();
	int i;
	for (i = 0; i < apCount; i++) {
		cJSON	*jObj = cJSON_CreateObject();

		cJSON_AddStringToObject(jObj, "ssid", (char *)apList[i].ssid);
		cJSON_AddNumberToObject(jObj, "chan", apList[i].primary);
		cJSON_AddNumberToObject(jObj, "rssi", apList[i].rssi);

		cJSON_AddItemToArray(ret->jResult, jObj);
	}
	wifiApRelease(apList);
}

static void _connect(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	// SSID is required
	const char	*ssid = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "ssid"));
	if (!ssid) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "SSID required";
		return;
	}

	// password is optional
	const char	*pass = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "pass"));

	if (wifiConnect(ssid, pass) != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Wifi failed to connect";
		return;
	}
}

static void _disconnect(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	if (wifiDisconnect() != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Wifi failed to disconnect";
		return;
	}
}

static void _status(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	wifiStatus_t	stat;
	if (wifiStatus(&stat) != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Wifi failed to read wifi status";
		return;
	}
	ret->jResult = cJSON_CreateObject();
	cJSON_AddBoolToObject(ret->jResult, "connected", stat.sta.connected);
	cJSON_AddBoolToObject(ret->jResult, "ip_assigned", stat.sta.ipAssigned);
	if (stat.sta.ipAssigned) {
		cJSON_AddStringToObject(ret->jResult, "ip_addr", stat.sta.ipAddr);
		cJSON_AddStringToObject(ret->jResult, "gw_addr", stat.sta.gwAddr);
		cJSON_AddStringToObject(ret->jResult, "ip_mask", stat.sta.ipMask);
	}
}

static cmdTab_t	cmdTab[] = {
	{"wifi-scan",		_scan},
	{"wifi-connect",	_connect},
	{"wifi-disconnect",	_disconnect},
	{"wifi-status",		_status},
};
static const int cmdTabSz = sizeof(cmdTab) / sizeof(cmdTab_t);

esp_err_t wifiCmdInit(void)
{
	ctrl_t *pCtrl = ctrl;
	if (pCtrl) {
		return ESP_OK;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	esp_err_t	status;
	if ((status = cmdFuncTabRegister(cmdTab, cmdTabSz, pCtrl)) != ESP_OK) {
		return status;
	}

	pCtrl->isInitialized = true;
	ctrl = pCtrl;
	return ESP_OK;
}
