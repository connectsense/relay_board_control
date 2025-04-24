/*
 * gpio_cmd.c
 *
 */
#include <string.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "cJSON.h"

#include "cmd_proc.h"
#include "gpio_cmd.h"

#define NUM_GPIO_PINS	(49)
#define GLITCH_MS		(50)

#define TIME_MS()		((uint32_t)(esp_timer_get_time() / 1000LL))

typedef enum {
	pinState_low = 0,
	pinState_high,
	pinState_rising,
	pinState_falling
} inputState_t;

typedef enum {
	pinDir_input,
	pinDir_inputPullup,
	pinDir_output
} pinDir_t;

typedef uint32_t glitchMs_t;

typedef struct {
	bool			enabled;
	pinDir_t		dir;
	inputState_t	state;
	uint32_t		cosTimeMs;  // Change of state timestamp
	glitchMs_t		glitchMs;
} pinCtrl_t;

typedef struct {
	bool		isInitialized;
	bool		isRunning;
	pinCtrl_t	pinCtrl[NUM_GPIO_PINS];
} ctrl_t;

static void input_scan(void* params);
static esp_err_t register_cmds(ctrl_t* pCtrl);

static ctrl_t* ctrl;

esp_err_t gpioCmdInit(void)
{
	ctrl_t* pCtrl = ctrl;
	if (pCtrl) {
		return ESP_OK;
	}

	pCtrl = calloc(1, sizeof(*pCtrl));
	if (!pCtrl) {
		return ESP_ERR_NO_MEM;
	}

	// ToDo - Other initializations ?

	// Register methods with the command processor
	esp_err_t	status;
	if ((status = register_cmds(pCtrl)) != ESP_OK) {
		return status;
	}

	pCtrl->isInitialized = true;
	ctrl = pCtrl;
	return ESP_OK;
}

esp_err_t gpioCmdStart(void)
{
	ctrl_t* pCtrl = ctrl;
	if (!pCtrl) {
		return ESP_ERR_INVALID_STATE;
	}
	if (pCtrl->isRunning) {
		return ESP_OK;
	}

	// Start the input monitor task
	BaseType_t	ret;
	ret = xTaskCreate(
		input_scan,
		"input_scan",
		2000,
		(void*)pCtrl,
		5,
		NULL
	);
	if (pdPASS != ret) {
		return ESP_FAIL;
	}

	// ToDo ?

	pCtrl->isRunning = true;
	return ESP_OK;
}

static void input_scan(void* params)
{
	ctrl_t* pCtrl = (ctrl_t *)params;
	uint32_t now_ms;
	pinCtrl_t* pin;
	gpio_num_t gpio_num;

	while (true)
	{
		vTaskDelay(pdMS_TO_TICKS(10));

		now_ms = TIME_MS();

		for (gpio_num = 0, pin = pCtrl->pinCtrl; gpio_num < NUM_GPIO_PINS; gpio_num++, pin++) {
			if (!pin->enabled || pin->dir == pinDir_output) {
				continue;
			}

			if (gpio_get_level(gpio_num) == 0) {
				// Input is low - check for change of state
				switch (pin->state)
				{
					case pinState_low:
						// Remain in this state
						break;

					case pinState_high:
						// Transitioning from high to low - start glitch timer
						pin->state = pinState_falling;
						pin->cosTimeMs = now_ms + GLITCH_MS;
						break;

					case pinState_falling:
						// Still low, see if glitch filter timeout reached
						if (pin->cosTimeMs <= now_ms) {
							// Officially low
							pin->state = pinState_low;
						}
						break;

					case pinState_rising:
						// Cancel rising state
						pin->state = pinState_low;
						break;

					default:
						// Got lost -- reset to low
						pin->state = pinState_low;
						break;
				}
			} else {
				// Input is high - check for change of state
				switch (pin->state)
				{
					case pinState_high:
						// Remain in this state
						break;

					case pinState_low:
						// Transitioning from low to high - start glitch timer
						pin->state = pinState_rising;
						pin->cosTimeMs = now_ms + GLITCH_MS;
						break;

					case pinState_rising:
						// Still high, see if glitch filter timeout reached
						if (pin->cosTimeMs <= now_ms) {
							// Officially high
							pin->state = pinState_high;
						}
						break;

					case pinState_falling:
						// Cancel falling state
						pin->state = pinState_high;
						break;

					default:
						// Got lost -- reset to low
						pin->state = pinState_low;
						break;
				}
			}
		}
	}
}

/**
 * @brief Helper function does common operations for called API methods
 */
static bool _enter_api(ctrl_t* pCtrl, cJSON *jParam, cmdReturn_t *ret, int* gpio_num)
{
	if (!pCtrl || !pCtrl->isRunning) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "GPIO service not running";
		return false;
	}

	cJSON* jObj = cJSON_GetObjectItem(jParam, "gpio_num");
	if (!cJSON_IsNumber(jObj)) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "gpio_num missing or invalid type";
		return false;
	}

	*gpio_num = jObj->valueint;
	if (*gpio_num < 0 || *gpio_num >= NUM_GPIO_PINS) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "Invalid GPIO number";
		return false;
	}

	return true;
}

/**
 * @brief Helper function to validate and return pin number with its pin control structure
 */
static pinCtrl_t* _get_pin_ctrl(ctrl_t* pCtrl, cJSON* jParam, cmdReturn_t* ret, int* gpio_num)
{
	if (!_enter_api(pCtrl, jParam, ret, gpio_num)) {
		return NULL;
	}

	pinCtrl_t* pin = &(pCtrl->pinCtrl[*gpio_num]);
	if (!pin->enabled) {
		ret->code = RPC_ERR_INTERNAL;
		ret->mesg = "pin not configured";
		return NULL;
	}

	return pin;
}

/**
 * /brief Process JSON command to configure a GPIO pin
 * 
 * JSON parameter contents:
 *   "gpio_num": <GPIO number>,
 *   "mode": <"in"|out">
 *   "pull_up_en": <true|false>,
 *   "pull_down_en": <true|false>
 * 
 */
static void _confPin(cJSON *jParam, cmdReturn_t *ret, void *cbData)
{
	ctrl_t* pCtrl = (ctrl_t*)cbData;
	int gpio_num;

	if (!_enter_api(pCtrl, jParam, ret, &gpio_num)) {
		return;
	}

	char* str;

	gpio_mode_t mode;
	str = cJSON_GetStringValue(cJSON_GetObjectItem(jParam, "mode"));
	if (!str) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "mode required";
		return;
	}
	if (strcmp(str, "in") == 0) {
		mode = GPIO_MODE_INPUT;
	} else if (strcmp(str, "out") == 0) {
		mode = GPIO_MODE_OUTPUT;
	} else {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "Invalid mode";
		return;
	}

	gpio_pullup_t pu_en = GPIO_PULLUP_DISABLE;
	str = cJSON_GetStringValue(cJSON_GetObjectItem(jParam, "pull_up_en"));
	if (str && strcmp(str, "true") == 0) {
		pu_en = GPIO_PULLUP_ENABLE;
	}

	gpio_pulldown_t pd_en = GPIO_PULLDOWN_DISABLE;
	str = cJSON_GetStringValue(cJSON_GetObjectItem(jParam, "pull_down_en"));
	if (str && strcmp(str, "true") == 0) {
		pd_en = GPIO_PULLDOWN_ENABLE;
	}

	if (GPIO_MODE_OUTPUT == mode) {
		// Set initial output level
		cJSON* jObj = cJSON_GetObjectItem(jParam, "istate");
		gpio_set_level((gpio_num_t)gpio_num, cJSON_IsTrue(jObj) ? 1 : 0);
	}

	gpio_config_t	gpioCfg = {
		.pin_bit_mask = (uint64_t)1 << gpio_num,
		.mode         = mode,
		.pull_down_en = pd_en,
		.pull_up_en   = pu_en,
		.intr_type    = GPIO_INTR_DISABLE
	};

	gpio_config(&gpioCfg);

	pinCtrl_t* pin = &pCtrl->pinCtrl[gpio_num];

	// Flag this as a configured pin
	pin->dir = (GPIO_MODE_OUTPUT == mode) ? pinDir_output : pinDir_input;
	pin->enabled = true;
}

/**
 * /brief Set output pin states
 * 
 * JSON parameter contents:
 *   "gpio_num"
 *   active: <"true"|"false">,
 */
static void _gpioSet(cJSON *jParam, cmdReturn_t *ret, void *cbData)
{
	ctrl_t* pCtrl = (ctrl_t*)cbData;
	int gpio_num;
	pinCtrl_t* pin = _get_pin_ctrl(pCtrl, jParam, ret, &gpio_num);
	if (!pin) {
		return;
	}

	cJSON* jObj = cJSON_GetObjectItem(jParam, "active");
	if (!cJSON_IsBool(jObj)) {
		ret->code = RPC_ERR_PARAMS;
		ret->mesg = "active missing or invalid type";
		return;
	}

	gpio_set_level((gpio_num_t)gpio_num, cJSON_IsTrue(jObj) ? 1 : 0);
}

/**
 * /brief Read input pin state
 * 
 * returns JSON structure:
 *   {"gpio_num": <number>, "active": <true|false>}
 */
static void _gpioGet(cJSON *jParam, cmdReturn_t *ret, void *cbData)
{
	ctrl_t* pCtrl = (ctrl_t*)cbData;
	int gpio_num;
	pinCtrl_t* pin = _get_pin_ctrl(pCtrl, jParam, ret, &gpio_num);
	if (!pin) {
		return;
	}

	bool level = (pin->state == pinState_high || pin->state == pinState_falling);

	ret->jResult = cJSON_CreateObject();
	cJSON_AddNumberToObject(ret->jResult, "gpio_num", gpio_num);
	cJSON_AddBoolToObject(ret->jResult, "active", level);
}

/**
 * /brief Return a JSON array of the state of all input pins
 * 
 * returns JSON array:
 *   [{"gpio_num": <int>, "active": <true|false>}, ...]
 */
static void _gpioGetAll(cJSON *jParam, cmdReturn_t *ret, void *cbData)
{
	ctrl_t* pCtrl = (ctrl_t*)cbData;

	ret->jResult = cJSON_CreateArray();

	int gpio_num;
	pinCtrl_t* pin;

	// Return the current states of all input pins
	for (gpio_num = 0, pin = pCtrl->pinCtrl; gpio_num < NUM_GPIO_PINS; gpio_num++, pin++) {
		if (!pin->enabled || pin->dir == pinDir_output) {
			continue;
		}

		bool active = (pin->state == pinState_high || pin->state == pinState_falling);

		cJSON* jItem = cJSON_CreateObject();
		cJSON_AddNumberToObject(jItem, "gpio_num", gpio_num);
		cJSON_AddBoolToObject(jItem, "active", active);
		cJSON_AddItemToArray(ret->jResult, jItem);
	}
}

static cmdTab_t	cmdTab[] = {
	{"gpio-conf",    _confPin},
	{"gpio-set",     _gpioSet},
	{"gpio-get",     _gpioGet},
	{"gpio-get-all", _gpioGetAll}
};
static const int cmdTabSz = sizeof(cmdTab) / sizeof(cmdTab_t);

static esp_err_t register_cmds(ctrl_t* pCtrl)
{
	esp_err_t	status;
	if ((status = cmdFuncTabRegister(cmdTab, cmdTabSz, pCtrl)) != ESP_OK) {
		return status;
	}
	return ESP_OK;
}
