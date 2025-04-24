/*
 * cmd_prod.h
 *
 *  Created on: Feb 14, 2023
 *      Author: wesd
 */

#ifndef COMPONENTS_CMD_PROC_INCLUDE_CMD_PROC_H_
#define COMPONENTS_CMD_PROC_INCLUDE_CMD_PROC_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "cJSON.h"
#include "test_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RPC_ERR_PARSE		(-32700)
#define RPC_ERR_INV_REQ		(-32600)
#define RPC_ERR_METHOD		(-32601)
#define RPC_ERR_PARAMS		(-32602)
#define RPC_ERR_INTERNAL	(-32603)


typedef struct {
	const char*	fwVersion;
} cmdConf_t;

typedef struct {
	int64_t		curTimeMs;
	char*		method;
	cJSON*		jParams;
} cmdRequest_t;

typedef struct {
	cJSON*		jResult;
	int			code;
	const char*	mesg;
	testComm_action_t tcAction;
} cmdReturn_t;

typedef void (*cmdFunc_t)(cJSON *jParam, cmdReturn_t *ret, void *cbData);

typedef struct {
	const char* method;
	cmdFunc_t	func;
} const cmdTab_t;

esp_err_t cmdProcInit(cmdConf_t* conf);

void cmdProcMesg(const char* mesg);

esp_err_t cmdFuncRegister(const char* method, cmdFunc_t func, void* cbData);

esp_err_t cmdFuncTabRegister(cmdTab_t* tab, int cmdTabSz, void* cbData);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_CMD_PROC_INCLUDE_CMD_PROC_H_ */
