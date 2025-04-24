/*
 * wifi_ctrl.c
 *
 *  Created on: Oct 24, 2022
 *      Author: wesd
 */
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "wifi_ctrl.h"

//static const char *TAG = "wifi_ctrl";

typedef struct {
	struct {
		esp_netif_t* sta;
	} netif;
	wifiStatus_t	status;
} wifiCtrl_t;

static wifiCtrl_t *wifiCtrl;

static void wifiEvtHandler(
	void *				evtArg,
	esp_event_base_t	evtBase,
	int32_t				evtId,
	void *				evtData
)
{
	wifiCtrl_t *pCtrl = (wifiCtrl_t *)evtArg;

	//printf("wifi event %ld\n", evtId);

	switch (evtId)
	{
	case WIFI_EVENT_STA_START:
		// ToDo
		break;

	case WIFI_EVENT_STA_CONNECTED:
		pCtrl->status.sta.connected = true;
		//printf("wifi connected\n");
		break;

	case WIFI_EVENT_STA_DISCONNECTED:
		//printf("wifi disconnected\n");
		pCtrl->status.sta.connected = false;
		pCtrl->status.sta.ipAssigned = false;
		break;

	default:
		break;
	}
}


static void ipEvtHandler(
	void *				evtArg,
	esp_event_base_t	evtBase,
	int32_t				evtId,
	void *				evtData
)
{
	wifiCtrl_t *pCtrl = (wifiCtrl_t *)evtArg;
	ip_event_got_ip_t *		evtGotIp;

	//printf("IP event %ld\n", evtId);

	switch (evtId)
	{
	case IP_EVENT_STA_GOT_IP:
		evtGotIp = (ip_event_got_ip_t *)evtData;

		//printf("IP address assigned\n");
		sprintf(pCtrl->status.sta.ipAddr, IPSTR, IP2STR(&evtGotIp->ip_info.ip));
		sprintf(pCtrl->status.sta.gwAddr, IPSTR, IP2STR(&evtGotIp->ip_info.gw));
		sprintf(pCtrl->status.sta.ipMask, IPSTR, IP2STR(&evtGotIp->ip_info.netmask));
		pCtrl->status.sta.ipAssigned = true;
		break;

	case IP_EVENT_STA_LOST_IP:
		pCtrl->status.sta.ipAssigned = false;
		break;

	default:
		break;
	}
}


esp_err_t wifiInit(void)
{
	wifiCtrl_t	*pCtrl = wifiCtrl;
	if (pCtrl) {
		return ESP_OK;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Set up handlers for wifi and ip events
	esp_event_handler_register(
		WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&wifiEvtHandler,
		(void *)pCtrl
	);

	esp_event_handler_register(
		IP_EVENT,
		ESP_EVENT_ANY_ID,
		&ipEvtHandler,
		(void *)pCtrl
	);

	// Register Wi-Fi API commands
	wifiCmdInit();

	wifiCtrl = pCtrl;
	return ESP_OK;
}

esp_err_t wifiStaStart(void)
{
	wifiCtrl_t	*pCtrl = wifiCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	pCtrl->netif.sta = esp_netif_create_default_wifi_sta();
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	// ESP32-S3 needs this for connections to APs to work
	esp_wifi_set_max_tx_power(40);

	return ESP_OK;
}


esp_err_t wifiStaStop(void)
{
	wifiCtrl_t	*pCtrl = wifiCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	if (pCtrl->status.sta.connected) {
		esp_wifi_disconnect();
		pCtrl->status.sta.connected = false;
		pCtrl->status.sta.ipAssigned = false;
	}

	esp_wifi_stop();
	esp_netif_destroy_default_wifi(pCtrl->netif.sta);
	return ESP_OK;
}

esp_err_t wifiConnect(const char *ssid, const char *pass)
{
	wifiCtrl_t	*pCtrl = wifiCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	wifi_config_t conf = {
		.sta = {
			.threshold.authmode = WIFI_AUTH_OPEN
		}
	};

	if (!ssid || strlen(ssid) > sizeof(conf.sta.ssid)) {
		return ESP_ERR_INVALID_ARG;
	}
	memcpy(conf.sta.ssid, (uint8_t *)ssid, strlen(ssid));

	if (pass) {
		if (strlen(pass) > sizeof(conf.sta.password)) {
			return ESP_ERR_INVALID_ARG;
		}
		memcpy(conf.sta.password, (uint8_t *)pass, strlen(pass));
	}

	pCtrl->status.sta.connected = false;
	pCtrl->status.sta.ipAssigned = false;

	esp_err_t	status;

	if ((status = esp_wifi_set_config(WIFI_IF_STA, &conf)) != ESP_OK) {
		return status;
	}

	if ((status = esp_wifi_connect()) != ESP_OK) {
		return status;
	}

	return ESP_OK;
}

esp_err_t wifiDisconnect(void)
{
	wifiCtrl_t	*pCtrl = wifiCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	if (pCtrl->status.sta.connected) {
		esp_wifi_disconnect();
		pCtrl->status.sta.connected = false;
		pCtrl->status.sta.ipAssigned = false;
	}

	return ESP_OK;
}

esp_err_t wifiStatus(wifiStatus_t *ret)
{
	wifiCtrl_t	*pCtrl = wifiCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	*ret = pCtrl->status;
	return ESP_OK;
}

