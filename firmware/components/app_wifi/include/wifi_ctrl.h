/*
 * wifi_ctrl.h
 *
 *  Created on: Oct 24, 2022
 *      Author: wesd
 */

#ifndef COMPONENTS_TF_WIFI_INCLUDE_WIFI_CTRL_H_
#define COMPONENTS_TF_WIFI_INCLUDE_WIFI_CTRL_H_

#include <esp_err.h>
#include <esp_wifi.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifiInit(void);

esp_err_t wifiStaStart(void);

esp_err_t wifiStaStop(void);

esp_err_t wifiConnect(const char *ssid, const char *pass);

esp_err_t wifiDisconnect(void);

typedef struct {
	struct {
		bool	connected;
		bool	ipAssigned;
		char	ipAddr[20];
		char	gwAddr[20];
		char	ipMask[20];
	} sta;
} wifiStatus_t;

esp_err_t wifiStatus(wifiStatus_t *ret);

esp_err_t wifiCmdInit(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_TF_WIFI_INCLUDE_WIFI_CTRL_H_ */
