#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp32/rom/crc.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "cJSON.h"

#include "watchdog.h"
#include "test_comm.h"

static const char* TAG = "test_comm";

// Message frame markers
#define MSG_SOH		((char)0x01)	// Start of header
#define MSG_STX		((char)0x02)	// Start of text
#define MSG_ETX		((char)0x03)	// End of text
#define MSG_EOT		((char)0x04)	// End of transmission

// Message receive states
typedef enum {
	msgState_idle = 0,	// Waiting for SOH
	msgState_hdr,		// Receiving header until STX
	msgState_body,		// Receiving message body until ETX
	msgState_crc,		// Receiving CRC until EOT
	msgState_err		// Recovering from error
} msgState_t;

#define MSG_HDR_SZ	(10)
#define MSG_BODY_SZ	(30000)
#define MSG_CRC_SZ	(10)
#define HTTP_RX_SZ	(8000)

typedef struct {
	testComm_conf_t	conf;
	bool			isRunning;
	int64_t			curTimeMs;
	char*			rxBuf;
	struct {
		msgState_t	state;
		char		hdr[MSG_HDR_SZ + 1];
		char		body[MSG_BODY_SZ + 1];
		char		crc[MSG_CRC_SZ];
		int			len;
		uint32_t	crc32;
	} msg;
	struct {
		bool		active;
		uint32_t	timeMs;
	} reboot;
} appCtrl_t;


static esp_err_t initUart(testComm_conf_t* conf);
static void commTask(void* param);
static void sendResponse(appCtrl_t* pCtrl, cJSON* jResp);
static void sendErrResponse(appCtrl_t* pCtrl, int errCode, const char* errMesg);

static appCtrl_t*	appCtrl;

esp_err_t testCommInit(testComm_conf_t* conf)
{
	if (appCtrl) {
		return ESP_OK;
	}

	// Sanity check some of the parameters
	if (!conf || !conf->cmdProc) {
		return ESP_ERR_INVALID_ARG;
	}

	appCtrl_t* pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	// Store the configuration
	pCtrl->conf = *conf;

	// Allocate the receive buffer
	if ((pCtrl->rxBuf = malloc(pCtrl->conf.rxBufSz)) == NULL) {
		return ESP_ERR_NO_MEM;
	}

	esp_err_t status;
	if ((status = watchdogInit()) != ESP_OK) {
		return status;
	}

	appCtrl = pCtrl;
	return ESP_OK;
}

esp_err_t testCommStart(void)
{
	appCtrl_t* pCtrl = appCtrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}
	if (pCtrl->isRunning) {
		return ESP_OK;
	}

	esp_err_t status;

	if ((status = initUart(&pCtrl->conf)) != ESP_OK) {
		return status;
	}

	// Start the application task
	BaseType_t	ret;
	ret = xTaskCreate(
		commTask,
		"test_comm",
		4000,
		(void*)pCtrl,
		pCtrl->conf.taskPriority,
		NULL
	);
	if (pdPASS != ret) {
		ESP_LOGE(TAG, "Task create failed");
		return ESP_FAIL;
	}

	// Start the watchdog timer
	watchdogStart();

	pCtrl->isRunning = true;
	return ESP_OK;
}

static esp_err_t enterAPI(appCtrl_t** pCtrl)
{
	*pCtrl = appCtrl;
	if (!*pCtrl || !(*pCtrl)->isRunning) {
		return ESP_ERR_INVALID_STATE;
	}

	return ESP_OK;
}

esp_err_t testCommSendResponse(cJSON* jResp, testComm_action_t* action)
{
	esp_err_t status;
	appCtrl_t* pCtrl;

	if ((status = enterAPI(&pCtrl)) != ESP_OK) {
		return status;
	}

	sendResponse(pCtrl, jResp);

	// Maybe schedule reboot
	pCtrl->reboot.active = action->reboot.active;
	pCtrl->reboot.timeMs = action->reboot.timeMs;

	if (action->newBaud > 0) {
		uart_port_t port = pCtrl->conf.uart.port;
		uart_wait_tx_done(port, pdMS_TO_TICKS(1000));
		uart_set_baudrate(port, action->newBaud);
		action->newBaud = 0;
	}

	return ESP_OK;
}

esp_err_t testCommSendErrResponse(int errCode, const char* errMesg)
{
	esp_err_t status;
	appCtrl_t* pCtrl;

	if ((status = enterAPI(&pCtrl)) != ESP_OK) {
		return status;
	}

	sendErrResponse(pCtrl, errCode, errMesg);
	return ESP_OK;
}

static esp_err_t initUart(testComm_conf_t* conf)
{
    uart_config_t uart_config = {
        .baud_rate = conf->uart.baud,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

	uart_port_t port = conf->uart.port;

    ESP_ERROR_CHECK(uart_driver_install(port, 1024 * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(port, conf->uart.gpio_txd, conf->uart.gpio_rxd, -1, -1));

    return ESP_OK;
}

static void sendMsg(appCtrl_t* pCtrl, const char* hdr, const char* body)
{
	static const char soh[1] = {MSG_SOH};
	static const char stx[1] = {MSG_STX};
	static const char etx[1] = {MSG_ETX};
	static const char eot[1] = {MSG_EOT};

	int	bodyLen = strlen(body);

	// Calc CRC32 of body and form hex string
	uint32_t	crc32;
	char		crcBuf[10];
	int			crcLen;
	crc32 = crc32_le(0, (uint8_t *)body, bodyLen);
	crcLen = sprintf(crcBuf, "%lx", crc32);

	uart_port_t  port = pCtrl->conf.uart.port;

	uart_write_bytes(port, soh, 1);
	uart_write_bytes(port, hdr, strlen(hdr));
	uart_write_bytes(port, stx, 1);
	uart_write_bytes(port, body, bodyLen);
	uart_write_bytes(port, etx, 1);
	uart_write_bytes(port, crcBuf, crcLen);
	uart_write_bytes(port, eot, 1);
}

static void sendResponse(appCtrl_t* pCtrl, cJSON* jResp)
{
	// Turn JSON object into a string
	char* resp = cJSON_PrintUnformatted(jResp);
	// Release memory used for JSON response object
	cJSON_Delete(jResp);
	// Send the response string
	sendMsg(pCtrl, "RESP", resp);
	// Release memory used for the string
	cJSON_free(resp);
}

static void sendErrResponse(appCtrl_t* pCtrl, int errCode, const char* errMesg)
{
	// Build the error object
	cJSON* jErr = cJSON_CreateObject();
	cJSON_AddNumberToObject(jErr, "code", errCode);
	cJSON_AddStringToObject(jErr, "message", errMesg);

	// Build the response object containing the error object
	cJSON* jResp = cJSON_CreateObject();
	cJSON_AddItemToObject(jResp, "error", jErr);

	// Send the response object
	sendResponse(pCtrl, jResp);
}

#if 0
static void procCmd(appCtrl_t* pCtrl)
{
	cJSON* jMsg = cJSON_Parse(pCtrl->msg.body);
	if (!jMsg) {
		sendErrResponse(pCtrl, RPC_ERR_PARSE, "Message not proper JSON");
		return;
	}

	// Build command request
	cmdRequest_t req;

	// method is required
	req.method = cJSON_GetStringValue(cJSON_GetObjectItem(jMsg, "method"));
	if (!req.method) {
		sendErrResponse(pCtrl, RPC_ERR_INV_REQ, "Missing 'method'");
		cJSON_Delete(jMsg);
		return;
	}

	// Some methods require parameters
	req.jParams = cJSON_GetObjectItem(jMsg, "params");

	// Some methods use current time
	req.curTimeMs = pCtrl->curTimeMs;

	pCtrl->newBaud = 0;

	cmdReturn_t ret;
	cmdProc(&req, &ret);

	if (0 == ret.code) {
		pCtrl->reboot.active = ret.reboot.active;
		pCtrl->reboot.timeMs = ret.reboot.timeMs;
		pCtrl->newBaud = ret.newBaud;

		// Set up the response JSON object
		cJSON *jResp = cJSON_CreateObject();

		if (ret.jResult) {
			// Returning information
			cJSON_AddItemToObject(jResp, "result", ret.jResult);
		} else {
			// Not returning information
			cJSON_AddNumberToObject(jResp, "result", 0);
		}

		sendResponse(pCtrl, jResp);

		if (pCtrl->newBaud > 0) {
			uart_wait_tx_done(COMM_PORT, pdMS_TO_TICKS(1000));
			vTaskDelay(pdMS_TO_TICKS(250));
			uart_driver_delete(COMM_PORT);
			initUart(pCtrl->newBaud);

			// Clear the state
			pCtrl->newBaud = 0;
		}
	} else {
		sendErrResponse(pCtrl, ret.code, ret.mesg);
	}

	// Clean up request message
	cJSON_Delete(jMsg);
}
#endif

static void procMsg(appCtrl_t *pCtrl)
{
	// At this point a properly-framed message has been received
	// so reset the watchdog now
	watchdogReset();

	if (strcmp("CMD", pCtrl->msg.hdr) == 0) {
		pCtrl->conf.cmdProc(pCtrl->msg.body);
	} else {
		sendMsg(pCtrl, "ERR", "Header not recognized");
	}
}


static void procData(appCtrl_t* pCtrl, char* rxBuf, int rxCount)
{
	static const char* hexDigits = "0123456789abcdef";

	int	i;
	for (i = 0; i < rxCount; i++) {
		char	c = *rxBuf++;

		switch (pCtrl->msg.state)
		{
		case msgState_idle:
			// Waiting for SOH
			if (MSG_SOH == c) {
				// Store the header
				pCtrl->msg.len = 0;
				pCtrl->msg.state = msgState_hdr;
			}
			break;

		case msgState_hdr:
			// Reading header
			if (MSG_STX == c) {
				// End of header, start receiving the message body
				pCtrl->msg.hdr[pCtrl->msg.len] = '\0';
				pCtrl->msg.len = 0;
				pCtrl->msg.state = msgState_body;
			} else if (MSG_SOH == c) {
				// Restart the header
				pCtrl->msg.len = 0;
			} else if (MSG_EOT == c) {
				// End the message early - discard it
				pCtrl->msg.state = msgState_idle;
			} else if (c < 0x20 || c > 0x7E) {
				// Bad character
				sendMsg(pCtrl, "ERR", "HDR-CHR: Illegal character in header");
				pCtrl->msg.state = msgState_err;
			} else if (pCtrl->msg.len < MSG_HDR_SZ) {
				pCtrl->msg.hdr[pCtrl->msg.len] = c;
				pCtrl->msg.len += 1;
			} else {
				// Header overflow
				sendMsg(pCtrl, "ERR", "HDR-OVR: Header too large");
				pCtrl->msg.state = msgState_err;
			}
			break;

		case msgState_body:
			if (MSG_ETX == c) {
				// Terminate the string
				pCtrl->msg.body[pCtrl->msg.len] = '\0';

				// Calculate CRC32 of the body
				pCtrl->msg.crc32 = crc32_le(0, (uint8_t *)pCtrl->msg.body, pCtrl->msg.len);

				// Receive the message CRC
				pCtrl->msg.state = msgState_crc;
				pCtrl->msg.len = 0;
			} else if (MSG_STX == c) {
				// Restart the message
				pCtrl->msg.len = 0;
			} else if (MSG_EOT == c) {
				// Early termination of message
				pCtrl->msg.state = msgState_idle;
			} else if (c < 0x20 || c > 0x7E) {
				// Bad character
				sendMsg(pCtrl, "ERR", "HDR-CHR: Illegal character in body");
				pCtrl->msg.state = msgState_err;
			} else if (pCtrl->msg.len < MSG_BODY_SZ) {
				// Add character to the message buffer
				pCtrl->msg.body[pCtrl->msg.len] = c;
				pCtrl->msg.len += 1;
			} else {
				// overflow
				sendMsg(pCtrl, "ERR", "MSG-OVR: Message body too large");
				pCtrl->msg.state = msgState_err;
			}
			break;

		case msgState_crc:
			c = tolower(c);
			if (MSG_EOT == c) {
				// Terminate the string
				pCtrl->msg.crc[pCtrl->msg.len] = '\0';

				// Compare CRCs
				uint32_t msgCrc = strtoul(pCtrl->msg.crc, NULL, 16);

				// If CRC is good, process the message
				if (msgCrc == pCtrl->msg.crc32) {
					procMsg(pCtrl);
				} else {
					sendMsg(pCtrl, "ERR", "CRC-FAIL: CRC check failed");
				}

				// Wait for the next message
				pCtrl->msg.state = msgState_idle;
			} else if (strchr(hexDigits, c) == NULL) {
				// Bad character
				sendMsg(pCtrl, "ERR", "CRC-CHR: Illegal character in CRC");
				pCtrl->msg.state = msgState_err;
			} else if (pCtrl->msg.len < MSG_CRC_SZ) {
				// Add character to the message buffer
				pCtrl->msg.crc[pCtrl->msg.len] = c;
				pCtrl->msg.len += 1;
			} else {
				// overflow
				sendMsg(pCtrl, "ERR", "CRC-OVR: CRC too large");
				pCtrl->msg.state = msgState_err;
			}
			break;

		case msgState_err:
			// Wait for EOT or SOH
			if (MSG_EOT == c) {
				// Discard this message and wait for the next
				pCtrl->msg.state = msgState_idle;
			} else if (MSG_SOH == c) {
				// Starting receiving the new header
				pCtrl->msg.state = msgState_hdr;
				pCtrl->msg.len = 0;
			}
			break;

		default:
			pCtrl->msg.state = msgState_err;
			break;
		}
	}
}


static void commTask(void* param)
{
	appCtrl_t* pCtrl = param;
	if (!pCtrl) {
		ESP_LOGE(TAG, "Control structure missing");
		vTaskDelete(NULL);
	}

	// Shorthand
	uart_port_t port = pCtrl->conf.uart.port;

    // Setup the task loop
	pCtrl->msg.state = msgState_idle;

    while (true) {
    	int	rxCount;

    	rxCount = uart_read_bytes(port, pCtrl->rxBuf, pCtrl->conf.rxBufSz, pdMS_TO_TICKS(100));

    	pCtrl->curTimeMs = esp_timer_get_time() / 1000LL;

    	if (rxCount > 0) {
    		//printf("%d bytes received\n", rxCount);
    		procData(pCtrl, pCtrl->rxBuf, rxCount);
    	}

    	if (pCtrl->reboot.active) {
    		if (pCtrl->curTimeMs >= pCtrl->reboot.timeMs) {
				if (pCtrl->conf.features.wifi) {
					//wifiDisconnect();	// ToDo --
				}
				if (pCtrl->conf.features.bluetooth) {
					// ToDo - shut down Blufi provisioning
				}
				vTaskDelay(pdMS_TO_TICKS(100));
    			// This does not return
    			esp_restart();
    		}
    	}
    }
}
