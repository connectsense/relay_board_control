/*
 * tf_http.c
 *
 *  Created on: Feb 14, 2023
 *      Author: wesd
 */
#include <stdbool.h>

#include "sdkconfig.h"

#include <esp_err.h>
#include <esp_log.h>

#include "http_cmd.h"
#include "tf_http.h"

static const char* TAG = "TF_HTTP";

typedef struct {
	tfHttpConf_t	conf;
	struct {
		esp_http_client_handle_t	handle;
		bool isOpen;
	} session;
} httpCtrl_t;

static httpCtrl_t	*httpCtrl;

esp_err_t tfHttpInit(tfHttpConf_t *conf)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (pCtrl) {
		return ESP_OK;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	pCtrl->conf = *conf;

	esp_err_t	status;
	tfHttpCmdConf_t cmdConf = {
		.rxBufSz = pCtrl->conf.rxBufSz
	};
	if ((status = httpCmdRegisterMethods(&cmdConf)) != ESP_OK) {
		return status;
	}

	httpCtrl = pCtrl;
	return ESP_OK;
}

esp_err_t tfHttpPost(tfHttpPostArgs_t* arg)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	if (!arg->url || !arg->data) {
		return ESP_ERR_INVALID_ARG;
	}

	esp_http_client_config_t conf = {
		.url = arg->url,
		.method = HTTP_METHOD_POST,
		.buffer_size = 2048,
		.buffer_size_tx = 2048,
		.timeout_ms = arg->timeoutMs
	};

	esp_http_client_handle_t	http;
	http = esp_http_client_init(&conf);
	if (!http) {
		ESP_LOGE(TAG, "esp_http_client_init failed");
		return ESP_FAIL;
	}

	// Add any headers
	int	i;
	for (i = 0; i < arg->hdrCt; i++) {
		esp_http_client_set_header(http, arg->hdr[i].name, arg->hdr[i].value);
	}

	esp_err_t	status = ESP_FAIL;

	if (esp_http_client_open(http, arg->dataLen) != ESP_OK) {
		ESP_LOGE(TAG, "esp_http_client_open error %x", status);
		goto exitHTTP;
	}

	if (arg->dataLen > 0) {
		if (esp_http_client_write(http, arg->data, arg->dataLen) < 0) {
			ESP_LOGE(TAG, "esp_http_client_write failed");
			goto exitHTTP;
		}
	}

	if (esp_http_client_fetch_headers(http) < 0) {
		ESP_LOGE(TAG, "esp_http_client_fetch_headers failed");
		goto exitHTTP;
	}

	arg->hStatus = esp_http_client_get_status_code(http);
	int rdLen = esp_http_client_get_content_length(http);

	if (rdLen > arg->rxLen) {
		ESP_LOGE(TAG, "content length (%d) > buffer size (%d)", rdLen, arg->rxLen);
		goto exitHTTP;
	}

	arg->rxLen = esp_http_client_read(http, arg->rxBuf, rdLen);
	if (arg->rxLen < 0) {
		ESP_LOGE(TAG, "esp_http_client_read failed");
		goto exitHTTP;
	}

	// If this point is reached all went well
	status = ESP_OK;

exitHTTP:
	esp_http_client_cleanup(http);
	return status;
}


esp_err_t tfHttpGet(tfHttpGetArgs_t* arg)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	if (!arg->url) {
		return ESP_ERR_INVALID_ARG;
	}

	esp_http_client_config_t conf = {
		.url = arg->url,
		.method = HTTP_METHOD_GET,
		.buffer_size = 2048,
		.buffer_size_tx = 2048,
		.timeout_ms = arg->timeoutMs
	};

	esp_http_client_handle_t	http;
	http = esp_http_client_init(&conf);
	if (!http) {
		return ESP_FAIL;
	}

	// Add any headers
	int	i;
	for (i = 0; i < arg->hdrCt; i++) {
		esp_http_client_set_header(http, arg->hdr[i].name, arg->hdr[i].value);
	}

	esp_err_t	status = ESP_OK;

	if (esp_http_client_open(http, 0) != ESP_OK) {
		status = ESP_FAIL;
		goto exitHTTP;
	}

	if (esp_http_client_fetch_headers(http) < 0) {
		status = ESP_FAIL;
		goto exitHTTP;
	}

	arg->hStatus = esp_http_client_get_status_code(http);
	int rdLen = esp_http_client_get_content_length(http);

	if (rdLen > arg->rxLen) {
		status = ESP_FAIL;
		goto exitHTTP;
	}

	arg->rxLen = esp_http_client_read(http, arg->rxBuf, rdLen);
	if (arg->rxLen < 0) {
		status = ESP_FAIL;
		goto exitHTTP;
	}

exitHTTP:
	esp_http_client_cleanup(http);
	return status;
}


esp_err_t tfHttpOpen(
	char*			url,
	esp_http_client_method_t method,
	size_t			wrLen,
	int				hdrCt,
	tfHttpHdr_t*	hdr
)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}
	if (!url) {
		return ESP_ERR_INVALID_ARG;
	}

	if (pCtrl->session.isOpen) {
		(void)esp_http_client_cleanup(pCtrl->session.handle);
		pCtrl->session.isOpen = false;
	}

	esp_http_client_config_t conf = {
		.url = url,
		.method = method,
		.buffer_size = 2048,
		.buffer_size_tx = 2048,
	};

	esp_http_client_handle_t http;
	http = esp_http_client_init(&conf);
	if (!http) {
		return ESP_FAIL;
	}

	// Add any headers
	int	i;
	for (i = 0; i < hdrCt; i++) {
		esp_http_client_set_header(http, hdr[i].name, hdr[i].value);
	}

	esp_err_t	ret;
	ret = esp_http_client_open(http, wrLen);

	if (ret == ESP_OK) {
		pCtrl->session.handle = http;
		pCtrl->session.isOpen = true;
	} else {
		esp_http_client_cleanup(http);
	}

	return ret;
}

esp_err_t tfHttpClose(void)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}

	if (pCtrl->session.isOpen) {
		esp_http_client_close(pCtrl->session.handle);
		esp_http_client_cleanup(pCtrl->session.handle);
		pCtrl->session.isOpen = false;
	}

	return ESP_OK;
}

esp_err_t tfHttpWrite(const char* data, int len)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (!pCtrl || !pCtrl->session.isOpen) {
		return ESP_ERR_INVALID_STATE;
	}

	int ret = esp_http_client_write(pCtrl->session.handle, data, len);
	return (ret == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t tfHttpWriteFinish(int* respLen, int* hStatus)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (!pCtrl || !pCtrl->session.isOpen) {
		return ESP_ERR_INVALID_STATE;
	}

	*respLen = esp_http_client_fetch_headers(pCtrl->session.handle);
	*hStatus = esp_http_client_get_status_code(pCtrl->session.handle);

	return (*respLen >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t tfHttpRead(char* buf, int* len)
{
	httpCtrl_t	*pCtrl = httpCtrl;
	if (!pCtrl || !pCtrl->session.isOpen) {
		return ESP_ERR_INVALID_STATE;
	}

	*len = esp_http_client_read(pCtrl->session.handle, buf, *len);

	return (*len >= 0) ? ESP_OK : ESP_FAIL;
}
