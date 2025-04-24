/*
 * tf_gpio.c
 *
 *  Created on: Nov 8, 2022
 *      Author: wesd
 */
#include <string.h>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "cmd_proc.h"
#include "nvs_cmd.h"

nvs_params_t nvs_params;

typedef struct {
	bool	isInitialized;
	bool	isRunning;
} ctrl_t;

static char* param_ns = "params";

nvs_params_t nvs_params;
static ctrl_t	*ctrl;

/**
 * /brief Set NVS value
 * 
 * JSON parameter contents:
 *   TBD
 * 
 */
static void _nvsSet(cJSON *jParam, cmdReturn_t *ret, void *cbData)
{
	ctrl_t* pCtrl = (ctrl_t *)cbData;
	if (!pCtrl || !pCtrl->isRunning) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Service not running";
	}

    nvs_handle_t handle;

    if (nvs_open(param_ns, NVS_READWRITE, &handle) != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Failed to open NVS";
        return;
    }

    char* str;

	str = cJSON_GetStringValue(cJSON_GetObjectItem(jParam, "unit_sn"));
    if (str) {
        if (nvs_set_str(handle, "unit_sn", str) != ESP_OK) {
            ret->code = RPC_ERR_INTERNAL;
            ret->mesg = "Failed to set parameter";
            goto nvsExit;
        }

        if (nvs_params.unit_sn != NULL) {
            free(nvs_params.unit_sn);
        }
        nvs_params.unit_sn = strdup(str);
    }

	str = cJSON_GetStringValue(cJSON_GetObjectItem(jParam, "tty_sn"));
    if (str) {
        if (nvs_set_str(handle, "tty_sn", str) != ESP_OK) {
            ret->code = RPC_ERR_INTERNAL;
            ret->mesg = "Failed to set parameter";
            goto nvsExit;
        }

        if (nvs_params.tty_sn != NULL) {
            free(nvs_params.tty_sn);
        }
        nvs_params.tty_sn = strdup(str);
    }

	nvs_commit(handle);
nvsExit:
    nvs_close(handle);
}

/**
 * /brief Get NVS parameters
 * 
 * JSON parameter contents:
 *   TBD
 * 
 * Returns:
 */
static void _nvsGet(cJSON *jParam, cmdReturn_t *ret, void *cbData)
{
	ctrl_t* pCtrl = (ctrl_t *)cbData;
	if (!pCtrl || !pCtrl->isRunning) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Service not running";
	}

    ret->jResult = cJSON_CreateObject();
	if (nvs_params.unit_sn) {
		cJSON_AddStringToObject(ret->jResult, "unit_sn", nvs_params.unit_sn);
	} else {
		cJSON_AddNullToObject(ret->jResult, "unit_sn");
	}

	if (nvs_params.tty_sn) {
		cJSON_AddStringToObject(ret->jResult, "tty_sn", nvs_params.tty_sn);
	} else {
		cJSON_AddNullToObject(ret->jResult, "tty_sn");
	}
}

static cmdTab_t	cmdTab[] = {
	{"nvs-set",	_nvsSet},
	{"nvs-get",	_nvsGet}
};
static const int cmdTabSz = sizeof(cmdTab) / sizeof(cmdTab_t);

static char* _loadStrParam(nvs_handle_t handle, const char* name)
{
    size_t valSize;
	char* value;

	if (nvs_get_str(handle, name, NULL, &valSize) == ESP_OK) {
        value = malloc(valSize);
        nvs_get_str(handle, name, value, &valSize);
    } else {
        // Ensure non-null value for the parameter
        value = strdup("");
    }

	return value;
}


static esp_err_t _loadParams(nvs_handle_t handle)
{
	nvs_params.unit_sn = _loadStrParam(handle, "unit_sn");
	nvs_params.tty_sn = _loadStrParam(handle, "tty_sn");

    return ESP_OK;
}

esp_err_t nvsCmdInit(void)
{
	ctrl_t* pCtrl = ctrl;
	if (pCtrl) {
		return ESP_OK;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	// Load parameters
	nvs_handle_t handle;

	esp_err_t status;

	status = nvs_open(param_ns, NVS_READONLY, &handle);
	if (ESP_ERR_NVS_NOT_FOUND == status) {
		// First time - must create the namespace
		status = nvs_open(param_ns, NVS_READWRITE, &handle);
	}
	if (ESP_OK != status) {
		return status;
	}

    status = _loadParams(handle);
    nvs_close(handle);

    if (ESP_OK != status) {
        return status;
    }

	// Register methods with the command processor
	if ((status = cmdFuncTabRegister(cmdTab, cmdTabSz, pCtrl)) != ESP_OK) {
		return status;
	}

	pCtrl->isInitialized = true;
	ctrl = pCtrl;
	return ESP_OK;
}

esp_err_t nvsCmdStart(void)
{
	ctrl_t* pCtrl = ctrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}
	if (pCtrl->isRunning) {
		return ESP_OK;
	}

	// ToDo ?

	pCtrl->isRunning = true;
	return ESP_OK;
}