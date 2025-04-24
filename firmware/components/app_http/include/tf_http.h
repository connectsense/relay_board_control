/*
 * tf_http.c
 *
 *  Created on: Feb 14, 2023
 *      Author: wesd
 */

#ifndef COMPONENTS_TF_HTTP_INCLUDE_TF_HTTP_H_
#define COMPONENTS_TF_HTTP_INCLUDE_TF_HTTP_H_

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>
#include <esp_http_client.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int	rxBufSz;
} tfHttpConf_t;

typedef struct {
	char	*name;
	char	*value;
} tfHttpHdr_t;


esp_err_t tfHttpInit(tfHttpConf_t *conf);

typedef struct {
	char		*url;
	int			hdrCt;
	tfHttpHdr_t	*hdr;
	int			dataLen;
	char		*data;
	int			hStatus;
	char		*rxBuf;
	int			rxLen;
	int			timeoutMs;
} tfHttpPostArgs_t;

esp_err_t tfHttpPost(tfHttpPostArgs_t* arg);

typedef struct {
	char		*url;
	int			hdrCt;
	tfHttpHdr_t	*hdr;
	int			hStatus;
	char		*rxBuf;
	int			rxLen;
	int			timeoutMs;
} tfHttpGetArgs_t;

esp_err_t tfHttpGet(tfHttpGetArgs_t* arg);

esp_err_t tfHttpOpen(
	char*			url,
	esp_http_client_method_t method,
	size_t			wrLen,
	int				hdrCt,
	tfHttpHdr_t*	hdr
);

esp_err_t tfHttpClose(void);

esp_err_t tfHttpWrite(const char* data, int len);

esp_err_t tfHttpWriteFinish(int* respLen, int* hStatus);

esp_err_t tfHttpRead(char* buf, int* len);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_TF_HTTP_INCLUDE_TF_HTTP_H_ */
