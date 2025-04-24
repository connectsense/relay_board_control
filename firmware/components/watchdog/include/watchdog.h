/*
 * watchdog.h
 *
 *  Created on: Oct 20, 2022
 *      Author: wesd
 */

#ifndef COMPONENTS_WATCHDOG_INCLUDE_WATCHDOG_H_
#define COMPONENTS_WATCHDOG_INCLUDE_WATCHDOG_H_

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t watchdogInit(void);

esp_err_t watchdogStart(void);

esp_err_t watchdogArm(void);

esp_err_t watchdogReset(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_WATCHDOG_INCLUDE_WATCHDOG_H_ */
