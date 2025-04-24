/*
 * nvs_cmd.h
 *
 */

#ifndef COMPONENTS_MAIN_INCLUDE_NVS_CMD_H_
#define COMPONENTS_MAIN_INCLUDE_NVS_CMD_H_

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char*  unit_sn; // Serial number of this fixture
    char*  tty_sn;  // Serial number of associated TTY device
} nvs_params_t;

esp_err_t nvsCmdInit(void);
esp_err_t nvsCmdStart(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_MAIN_INCLUDE_NVS_CMD_H_ */
