/*
 * cmd_proc.c
 *
 *  Created on: Feb 14, 2023
 *      Author: wesd
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/freeRTOS.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "cJSON.h"

#include "cmd_proc.h"
#include "test_comm.h"

static const char* TAG = "cmd_proc";

#define MUTEX_GET(ctrl)	xSemaphoreTake(ctrl->mutex, 0xFFFFFFFF)
#define MUTEX_PUT(ctrl)	xSemaphoreGive(ctrl->mutex)

typedef struct cmdTabItem_s {
	struct cmdTabItem_s	*next;
	const char			*method;
	cmdFunc_t			func;
	void				*cbData;
} cmdListItem_t;

typedef struct {
	cmdListItem_t	*head;
	cmdListItem_t	*tail;
} cmdList_t;

//#define HTTP_RX_SIZE	(8000)

typedef struct {
	cmdConf_t			conf;
	SemaphoreHandle_t	mutex;
	cmdList_t			cmdList;
} cmdCtrl_t;

static void cmdProc(cmdRequest_t* req, cmdReturn_t* ret);

static cmdCtrl_t	*cmdCtrl;

esp_err_t cmdProcInit(cmdConf_t* conf)
{
	cmdCtrl_t	*pCtrl = cmdCtrl;
	if (cmdCtrl) {
		return ESP_OK;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}
	pCtrl->conf = *conf;

	pCtrl->mutex = xSemaphoreCreateMutex();

	cmdCtrl = pCtrl;
	return ESP_OK;
}

void cmdProcMesg(const char* mesg)
{
	cJSON* jMsg = cJSON_Parse(mesg);
	if (!jMsg) {
		testCommSendErrResponse(RPC_ERR_PARSE, "Message not proper JSON");
		return;
	}

	// Build command request
	cmdRequest_t req;

	// method is required
	req.method = cJSON_GetStringValue(cJSON_GetObjectItem(jMsg, "method"));
	if (!req.method) {
		testCommSendErrResponse(RPC_ERR_INV_REQ, "Missing 'method'");
		cJSON_Delete(jMsg);
		return;
	}

	// Some methods require parameters
	req.jParams = cJSON_GetObjectItem(jMsg, "params");

	// Some methods use current time
	req.curTimeMs = esp_timer_get_time() / 1000LL;

	// Set inactive return values, command process may change any of these
	cmdReturn_t ret = {
		.tcAction = testComm_action_init()
	};

	cmdProc(&req, &ret);

	// Done with request message
	cJSON_Delete(jMsg);

	if (0 != ret.code) {
		testCommSendErrResponse(ret.code, ret.mesg);
		return;
	}

	// Set up the response JSON object
	cJSON* jResp = cJSON_CreateObject();

	if (ret.jResult) {
		// Returning information
		cJSON_AddItemToObject(jResp, "result", ret.jResult);
	} else {
		// Not returning information
		cJSON_AddNumberToObject(jResp, "result", 0);
	}

	testCommSendResponse(jResp, &ret.tcAction);
}

static cmdListItem_t *findCmdItem(cmdCtrl_t *pCtrl, const char *method)
{
	cmdListItem_t	*item;

	for (item = pCtrl->cmdList.head; item != NULL; item = item->next) {
		if (strcmp(method, item->method) == 0) {
			return item;
		}
	}

	// No match found
	return NULL;
}


esp_err_t cmdFuncRegister(const char* method, cmdFunc_t func, void* cbData)
{
	cmdCtrl_t* pCtrl = cmdCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	if (!method || !func || strlen(method) == 0) {
		return ESP_ERR_INVALID_ARG;
	}

	esp_err_t	status = ESP_OK;

	MUTEX_GET(pCtrl);

	// Check if method is already registered
	cmdListItem_t* item = findCmdItem(pCtrl, method);
	if (NULL == item) {
		// Add item to the linked list
		item = malloc(sizeof(*item));
		if (!item) {
			MUTEX_PUT(pCtrl);
			return ESP_ERR_NO_MEM;
		}

		item->next = NULL;
		item->method = method;
		item->func = func;
		item->cbData = cbData;

		if (pCtrl->cmdList.head == NULL) {
			// First item in the list
			pCtrl->cmdList.head = item;
		} else {
			// Add item to the tail
			pCtrl->cmdList.tail->next = item;
		}
		pCtrl->cmdList.tail = item;
	} else {
		// A method by that name is already registered
		ESP_LOGE(TAG, "Method \"%s\" already registered", method);
		status = ESP_FAIL;
	}

	MUTEX_PUT(pCtrl);
	return status;
}


esp_err_t cmdFuncTabRegister(cmdTab_t *tab, int cmdTabSz, void *cbData)
{
	esp_err_t	status;
	int			i;

	for (i = 0; i < cmdTabSz; i++, tab++) {
		status = cmdFuncRegister(tab->method, tab->func, cbData);
		if (ESP_OK != status) {
			return status;
		}
	}
	return ESP_OK;
}

static const char* chipModelStr(esp_chip_model_t model)
{
	switch (model)
	{
		case CHIP_ESP32:
			return "ESP32";
		case CHIP_ESP32S2:
			return "ESP32S2";
		case CHIP_ESP32S3:
			return "ESP32S3";
		case CHIP_ESP32C3:
			return "ESP32C3";
		case CHIP_ESP32H2:
			return "ESP32H2";
		default:
			return "?";
	}
}

static void cmdProc(cmdRequest_t* req, cmdReturn_t* ret)
{
	ret->code = 0;
	ret->mesg = "";
	ret->jResult = NULL;

	cmdCtrl_t	*pCtrl = cmdCtrl;
	if (!pCtrl) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Command processor not initialized";
		return;
	}

	if (!req->method || strlen(req->method) == 0) {
		ret->code = RPC_ERR_METHOD;
		ret->mesg = "Invalid method string";
		return;
	}

	if (strcmp("version", req->method) == 0) {
		ret->jResult = cJSON_CreateObject();
		cJSON_AddStringToObject(ret->jResult, "version", pCtrl->conf.fwVersion);
	} else if (strcmp("uptime", req->method) == 0) {
		ret->jResult = cJSON_CreateObject();
		cJSON_AddNumberToObject(ret->jResult, "uptime", (req->curTimeMs / 1000LL));
	} else if (strcmp("reboot", req->method) == 0) {
		ret->tcAction.reboot.active = true;
		ret->tcAction.reboot.timeMs = req->curTimeMs + 500;
	} else if (strcmp("echo", req->method) == 0) {
		char* data = cJSON_GetStringValue(cJSON_GetObjectItem(req->jParams, "data"));
		ret->jResult = cJSON_CreateObject();
		if (data) {
			cJSON_AddStringToObject(ret->jResult, "data", data);
		} else {
			cJSON_AddStringToObject(ret->jResult, "data", "");
		}
	} else if (strcmp("chip-info", req->method) == 0) {
		esp_chip_info_t	info;
		esp_chip_info(&info);
		ret->jResult = cJSON_CreateObject();
		cJSON_AddNumberToObject(ret->jResult, "model", info.model);
		cJSON_AddStringToObject(ret->jResult, "name", chipModelStr(info.model));
		cJSON_AddNumberToObject(ret->jResult, "revision", info.revision);
		cJSON_AddNumberToObject(ret->jResult, "cores", info.cores);
	} else if (strcmp("set-baud", req->method) == 0) {
		cJSON*	jBaud = cJSON_GetObjectItem(req->jParams, "value");
		if (cJSON_IsNumber(jBaud)) {
			ret->tcAction.newBaud = (uint32_t)jBaud->valueint;
		} else {
			ret->code = RPC_ERR_PARAMS;
			ret->mesg = "Baud value required";
		}
	} else {
		// Check for registered commands
		MUTEX_GET(pCtrl);
		cmdListItem_t* item = findCmdItem(pCtrl, req->method);
		if (item == NULL) {
			ret->code = RPC_ERR_METHOD;
			ret->mesg = "Method not supported";
		} else {
			item->func(req->jParams, ret, item->cbData);
		}
		MUTEX_PUT(pCtrl);
	}
}
