/*
 * gpio_cmd.h
 *
 */

#ifndef COMPONENTS_MAIN_INCLUDE_GPIO_CMD_H_
#define COMPONENTS_MAIN_INCLUDE_GPIO_CMD_H_


#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gpioCmdInit(void);
esp_err_t gpioCmdStart(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MAIN_INCLUDE_GPIO_CMD_H_ */
