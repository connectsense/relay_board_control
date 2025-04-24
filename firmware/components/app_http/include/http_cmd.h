/*
 * http_cmd.h
 *
 *  Created on: Feb 20, 2023
 *      Author: wesd
 */

#ifndef COMPONENTS_TF_HTTP_INCLUDE_HTTP_CMD_H_
#define COMPONENTS_TF_HTTP_INCLUDE_HTTP_CMD_H_

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int	rxBufSz;
} tfHttpCmdConf_t;

esp_err_t httpCmdRegisterMethods(tfHttpCmdConf_t *conf);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_TF_HTTP_INCLUDE_HTTP_CMD_H_ */
