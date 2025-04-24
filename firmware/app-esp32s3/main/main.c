#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_netif.h"

#include "test_comm.h"
#include "cmd_proc.h"
#include "gpio_cmd.h"
#include "nvs_cmd.h"
#include "wifi_ctrl.h"
#include "tf_http.h"

//static const char* TAG = "app_main";

extern const char* fwVersion;

// Configuration for test communications
static testComm_conf_t tcConf = {
	.uart = {
		.port = UART_NUM_0,
		.baud = 115200,
#if 0
		.gpio_rxd = GPIO_NUM_3,
		.gpio_txd = GPIO_NUM_1
#else
		// Use default UART0 GPIO pins
		.gpio_rxd = -1,
		.gpio_txd = -1
#endif
	},
	.rxBufSz = 2048,
	.taskPriority = 8,
	.cmdProc = cmdProcMesg
};

void app_main(void)
{
	esp_err_t	status;

	status = nvs_flash_init();
    if (status == ESP_ERR_NVS_NO_FREE_PAGES || status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        status = nvs_flash_init();
    }
    ESP_ERROR_CHECK(status);

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_netif_init());

    // Initialize command processor before test components
	cmdConf_t cpConf;
	cpConf.fwVersion = fwVersion;
    ESP_ERROR_CHECK(cmdProcInit(&cpConf));
	ESP_ERROR_CHECK(testCommInit(&tcConf));

	ESP_ERROR_CHECK(nvsCmdInit());
	ESP_ERROR_CHECK(gpioCmdInit());
	ESP_ERROR_CHECK(wifiInit());

	tfHttpConf_t httpConf = {
		.rxBufSz = 2048
	};
	ESP_ERROR_CHECK(tfHttpInit(&httpConf));

	// Start things
	ESP_ERROR_CHECK(nvsCmdStart());
	ESP_ERROR_CHECK(gpioCmdStart());
	ESP_ERROR_CHECK(wifiStaStart());

	// Finally start communications
	ESP_ERROR_CHECK(testCommStart());
}
