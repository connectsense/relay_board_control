/*
 * watchdog.c
 *
 *  Created on: Oct 20, 2022
 *      Author: wesd
 */
#include <stdbool.h>
#include <unistd.h>

#include "sdkconfig.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "watchdog.h"

#define WD_TIME_LIMIT_SECS		(30UL * 60UL)	// 30 minutes
#define CUR_TIME_SECS			((uint32_t)(esp_timer_get_time() / 1000000LL))

typedef struct {
	bool		isRunning;
	uint32_t	endTime;
} wdCtrl_t;

static void wdTask(void *param);


static wdCtrl_t	*wdCtrl;

esp_err_t watchdogInit(void)
{
	wdCtrl_t	*pCtrl = wdCtrl;
	if (pCtrl) {
		return ESP_OK;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	wdCtrl = pCtrl;
	return ESP_OK;
}

esp_err_t watchdogStart(void)
{
	wdCtrl_t	*pCtrl = wdCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}
	if (pCtrl->isRunning) {
		return ESP_OK;
	}

	BaseType_t	ret;
	ret = xTaskCreate(
		wdTask,
		"app_watchdog",
		2000,
		(void *)pCtrl,
		10,
		NULL
	);
	if (pdPASS != ret) {
		return ESP_FAIL;
	}

	pCtrl->isRunning = true;
	return ESP_OK;
}

static void _reset(wdCtrl_t *pCtrl)
{
	pCtrl->endTime = (esp_timer_get_time() / 1000000LL) + WD_TIME_LIMIT_SECS;
}

esp_err_t watchdogReset(void)
{
	wdCtrl_t	*pCtrl = wdCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	_reset(pCtrl);
	return ESP_OK;
}

static void wdTask(void* param)
{
	wdCtrl_t* pCtrl = param;

	// Set initial timeout
	_reset(pCtrl);

	while (true)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));

		uint32_t curTime = CUR_TIME_SECS;

		if (curTime > pCtrl->endTime) {
			// This function does not return
			esp_restart();
		}
	}
}
