/*
 * test_comm.h
 *
 *  Created on: Nov 8, 2022
 *      Author: wesd
 */

#ifndef COMPONENTS_TEST_COMM_INCLUDE_TEST_COMM_H_
#define COMPONENTS_TEST_COMM_INCLUDE_TEST_COMM_H_


#include "esp_err.h"
#include "driver/uart.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cmdProcFunc_t)(const char* mesg);

typedef struct {
    struct {
        uart_port_t port;
        int         gpio_txd;
        int         gpio_rxd;
        uint32_t    baud;
    } uart;
    UBaseType_t     taskPriority;
    int             rxBufSz;
    cmdProcFunc_t   cmdProc;
    struct {
        uint32_t    wifi: 1;
        uint32_t    bluetooth: 1;
    } features;
} testComm_conf_t;

// Data passed back to testComm by command processing
typedef struct {
    uint32_t    newBaud;
    struct {
        bool    active;
        uint32_t timeMs;   
    } reboot;
} testComm_action_t;

#define testComm_action_init()  {.newBaud = 0, .reboot.active = false, .reboot.timeMs = 0}

esp_err_t testCommInit(testComm_conf_t* conf);
esp_err_t testCommStart(void);
esp_err_t testCommSendResponse(cJSON* jResp, testComm_action_t* action);
esp_err_t testCommSendErrResponse(int errCode, const char* errMesg);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_TEST_COMM_INCLUDE_TEST_COMM_H_ */
