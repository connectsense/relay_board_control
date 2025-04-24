/*
 * http_cmd.c
 *
 *  Created on: Feb 20, 2023
 *      Author: wesd
 */
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>
#include <mbedtls/base64.h>

#include "cmd_proc.h"
#include "tf_http.h"
#include "http_cmd.h"

typedef struct {
	tfHttpCmdConf_t	conf;
	char			*rxBuf;
} ctrl_t;

static ctrl_t *ctrl;

static bool _enterApi(void *cbData, cmdReturn_t *ret, ctrl_t **ppCtrl)
{
	*ppCtrl = cbData;
	if (!*ppCtrl) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "tf_http not initialized";
		return false;
	}
	return true;
}

static void _postBin(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	char *url = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "url"));
	if (!url) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'url' missing";
		return;
	}

	// Expecting Base64-encoded binary data
	char *src = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "data"));
	if (!src) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'data' missing";
		return;
	}

	size_t	srcLen = strlen(src);
	size_t	outSz;

	// Determine size of buffer required to hold decoded binary
	int sts;
	sts = mbedtls_base64_decode(NULL, 0, &outSz, (unsigned char*)src, srcLen);
	if (MBEDTLS_ERR_BASE64_INVALID_CHARACTER == sts) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'data' not proper Base64";
		return;
	}

	// Allocate buffer to hold decoded data
	char *outBuf = malloc(outSz);
	if (!outBuf) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Not enough memory";
		return;
	}

	// Decode
	size_t	outLen;
	mbedtls_base64_decode((unsigned char *)outBuf, outSz, &outLen, (unsigned char*)src, srcLen);

	// Get optional millisecond timeout, default = 20,000
	int tout;
	cJSON*	jObj = cJSON_GetObjectItem(jParams, "timeout_ms");
	if (jObj) {
		tout = jObj->valueint;
	} else {
		tout = 20000;
	}

	// Build headers
	tfHttpHdr_t* hdrs = NULL;
	int	hdrCt = 0;

	cJSON* h = cJSON_GetObjectItem(jParams, "headers");
	if (cJSON_IsArray(h)) {
		int	sz = cJSON_GetArraySize(h);
		hdrs = calloc(sz, sizeof(*hdrs));
		if (hdrs) {
			for (hdrCt = 0; hdrCt < sz; hdrCt++) {
				cJSON* item = cJSON_GetArrayItem(h, hdrCt);
				hdrs[hdrCt].name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
				hdrs[hdrCt].value = cJSON_GetStringValue(cJSON_GetObjectItem(item, "value"));
			}
		}
	} else {
		int	sz = 1;
		hdrs = calloc(sz, sizeof(*hdrs));

		if (hdrs) {
			hdrs[hdrCt].name = "Content-Type";
			hdrs[hdrCt].value ="application/octet-stream";
			hdrCt += 1;
		}
	}

	// Perform the post
	tfHttpPostArgs_t args = {
		.url = url,
		.hdrCt = hdrCt,
		.hdr = hdrs,
		.dataLen = outLen,
		.data = outBuf,
		.rxBuf = pCtrl->rxBuf,
		.rxLen = pCtrl->conf.rxBufSz - 1,
		.timeoutMs = tout
	};

	esp_err_t	status;
	status = tfHttpPost(&args);

	// Release the headers memory
	if (hdrs) {
		free(hdrs);
	}

	// Release the decoded data buffer
	free(outBuf);

	// Check post status
	if (ESP_OK != status) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "HTTP transaction failed";
		return;
	}
	pCtrl->rxBuf[args.rxLen] = 0;

	ret->jResult = cJSON_CreateObject();
	cJSON_AddNumberToObject(ret->jResult, "status_code", args.hStatus);
	if (args.rxLen > 0) {
		cJSON_AddStringToObject(ret->jResult, "text", pCtrl->rxBuf);
	}
}

static void _post(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	char *url = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "url"));
	if (!url) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'url' missing";
		return;
	}

	char *jStr = cJSON_PrintUnformatted(cJSON_GetObjectItem(jParams, "data"));
	if (!jStr) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'data' missing or not proper JSON";
		return;
	}

	// Get optional millisecond timeout, default = 20,000
	int tout;
	cJSON*	jObj = cJSON_GetObjectItem(jParams, "timeout_ms");
	if (jObj) {
		tout = jObj->valueint;
	} else {
		tout = 20000;
	}

	// Build headers
	tfHttpHdr_t* hdrs = NULL;
	int	hdrCt = 0;

	cJSON* h = cJSON_GetObjectItem(jParams, "headers");
	if (cJSON_IsArray(h)) {
		int	sz = cJSON_GetArraySize(h);
		hdrs = calloc(sz, sizeof(*hdrs));
		if (hdrs) {
			for (hdrCt = 0; hdrCt < sz; hdrCt++) {
				cJSON* item = cJSON_GetArrayItem(h, hdrCt);
				hdrs[hdrCt].name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
				hdrs[hdrCt].value = cJSON_GetStringValue(cJSON_GetObjectItem(item, "value"));
			}
		}
	} else {
		int	sz = 2;
		hdrs = calloc(sz, sizeof(*hdrs));

		if (hdrs) {
			hdrs[hdrCt].name = "Content-Type";
			hdrs[hdrCt].value ="application/json";
			hdrCt += 1;

			hdrs[hdrCt].name = "Accept";
			hdrs[hdrCt].value ="application/json";
			hdrCt += 1;
		}
	}

	// Perform the post
	tfHttpPostArgs_t args = {
		.url = url,
		.hdrCt = hdrCt,
		.hdr = hdrs,
		.dataLen = strlen(jStr),
		.data = jStr,
		.rxBuf = pCtrl->rxBuf,
		.rxLen = pCtrl->conf.rxBufSz - 1,
		.timeoutMs = tout
	};

	esp_err_t	status;
	status = tfHttpPost(&args);

	// Done with the headers memory
	if (hdrs) {
		free(hdrs);
	}

	// Release the JSON string
	cJSON_free(jStr);

	// Check post status
	if (ESP_OK != status) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "HTTP transaction failed";
		return;
	}
	pCtrl->rxBuf[args.rxLen] = 0;

	ret->jResult = cJSON_CreateObject();
	cJSON_AddNumberToObject(ret->jResult, "status_code", args.hStatus);
	if (args.rxLen > 0) {
		cJSON_AddRawToObject(ret->jResult, "text", pCtrl->rxBuf);
	}
}

static void _get(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	char *url = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "url"));
	if (!url) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'url' missing";
		return;
	}

	// Get optional millisecond timeout, default = 5,000
	int tout;
	cJSON*	jObj = cJSON_GetObjectItem(jParams, "timeout_ms");
	if (jObj) {
		tout = jObj->valueint;
	} else {
		tout = 5000;
	}

	// Build headers
	tfHttpHdr_t	*hdrs = NULL;
	int	hdrCt = 0;

	cJSON *h = cJSON_GetObjectItem(jParams, "headers");
	if (cJSON_IsArray(h)) {
		int	sz = cJSON_GetArraySize(h);

		hdrs = calloc(sz, sizeof(*hdrs));
		for (hdrCt = 0; hdrCt < sz; hdrCt++) {
			cJSON *item = cJSON_GetArrayItem(h, hdrCt);
			hdrs[hdrCt].name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
			hdrs[hdrCt].value = cJSON_GetStringValue(cJSON_GetObjectItem(item, "value"));
		}
	}

	tfHttpGetArgs_t args = {
		.url = url,
		.hdrCt = hdrCt,
		.hdr = hdrs,
		.rxBuf = pCtrl->rxBuf,
		.rxLen = pCtrl->conf.rxBufSz - 1,
		.timeoutMs = tout
	};

	esp_err_t	status;
	status = tfHttpGet(&args);

	if (hdrs) {
		free(hdrs);
	}

	// Check result of HTTP operation
	if (ESP_OK != status) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "HTTP transaction failed";
		return;
	}
	pCtrl->rxBuf[args.rxLen] = 0;

	ret->jResult = cJSON_CreateObject();
	cJSON_AddNumberToObject(ret->jResult, "status_code", args.hStatus);
	cJSON_AddRawToObject(ret->jResult, "text", pCtrl->rxBuf);
}

static void _open(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	char*	url = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "url"));
	char*	method = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "method"));
	cJSON*	jLen = cJSON_GetObjectItem(jParams, "wr_len");
	cJSON*	jHdr = cJSON_GetObjectItem(jParams, "hdr");

	if (!url || !method || !jLen) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "Missing parameter(s)";
		return;
	}

	esp_http_client_method_t hMethod;
	if (strcmp("post", method) == 0) {
		hMethod = HTTP_METHOD_POST;
	} else {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "HTTP method not supported";
		return;
	}

	tfHttpHdr_t*	hdrs = NULL;
	int				hdrCt = 0;

	int	arraySz = cJSON_GetArraySize(jHdr);
	if (arraySz > 0) {
		hdrs = calloc(arraySz, sizeof(*hdrs));
		if (!hdrs) {
			ret->code = RPC_ERR_INTERNAL;
			ret->mesg = "Not enough memory";
			return;
		}

		for (hdrCt = 0; hdrCt < arraySz; hdrCt++) {
			cJSON* jItem = cJSON_GetArrayItem(jHdr, hdrCt);

			hdrs[hdrCt].name = cJSON_GetStringValue(cJSON_GetObjectItem(jItem, "name"));
			hdrs[hdrCt].value = cJSON_GetStringValue(cJSON_GetObjectItem(jItem, "value"));
		}
	}

	esp_err_t status;
	status = tfHttpOpen(url, hMethod, jLen->valueint, hdrCt, hdrs);

	if (hdrs) {
		free(hdrs);
	}

	if (ESP_OK != status) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "HTTP open failed";
		return;
	}
}

static void _close(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	(void)tfHttpClose();
}

static void _wrBin(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	// Expecting Base64-encoded binary data
	char *src = cJSON_GetStringValue(cJSON_GetObjectItem(jParams, "data"));
	if (!src) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'data' missing";
		return;
	}

	size_t	srcLen = strlen(src);
	size_t	outSz;

	// Determine size of buffer required to hold decoded binary
	int sts;
	sts = mbedtls_base64_decode(NULL, 0, &outSz, (unsigned char*)src, srcLen);
	if (MBEDTLS_ERR_BASE64_INVALID_CHARACTER == sts) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "'data' not proper Base64";
		return;
	}

	// Allocate buffer to hold decoded data
	char *outBuf = malloc(outSz);
	if (!outBuf) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Not enough memory";
		return;
	}

	// Decode
	size_t	outLen;
	mbedtls_base64_decode((unsigned char *)outBuf, outSz, &outLen, (unsigned char*)src, srcLen);

	esp_err_t status;
	status = tfHttpWrite(outBuf, outLen);
	free(outBuf);

	if (status != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "HTTP write failed";
		return;
	}
}


static void _wrFinish(cJSON *jParams, cmdReturn_t *ret, void *cbData)
{
	ctrl_t *pCtrl;
	if (!_enterApi(cbData, ret, &pCtrl)) {
		return;
	}

	esp_err_t	status;
	int			respLen;
	int			hStatus;

	status = tfHttpWriteFinish(&respLen, &hStatus);
	if (status != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "HTTP finish failed";
		return;
	}

	int rxLen = pCtrl->conf.rxBufSz - 1;

	status = tfHttpRead(pCtrl->rxBuf, &rxLen);
	if (status != ESP_OK) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "HTTP read failed";
		return;
	}
	pCtrl->rxBuf[rxLen] = 0;

	ret->jResult = cJSON_CreateObject();
	cJSON_AddNumberToObject(ret->jResult, "status_code", hStatus);
	if (rxLen > 0) {
		cJSON_AddStringToObject(ret->jResult, "text", pCtrl->rxBuf);
	}
}

static cmdTab_t	cmdTab[] = {
	{"http-post-bin",	_postBin},
	{"http-post",		_post},
	{"http-get",		_get},
	{"http-open",		_open},
	{"http-close",		_close},
	{"http-write-bin",	_wrBin},
	{"http-write-fin",	_wrFinish},
};
static const int cmdTabSz = sizeof(cmdTab) / sizeof(cmdTab_t);

esp_err_t httpCmdRegisterMethods(tfHttpCmdConf_t *conf)
{
	ctrl_t *pCtrl = ctrl;
	if (pCtrl) {
		return ESP_OK;
	}

	if (conf->rxBufSz < 1) {
		return ESP_ERR_INVALID_ARG;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	pCtrl->conf = *conf;

	pCtrl->rxBuf = malloc(pCtrl->conf.rxBufSz);
	if (!pCtrl->rxBuf) {
		return ESP_ERR_NO_MEM;
	}

	esp_err_t	status;
	if ((status = cmdFuncTabRegister(cmdTab, cmdTabSz, pCtrl)) != ESP_OK) {
		return status;
	}

	ctrl = pCtrl;
	return ESP_OK;
}
