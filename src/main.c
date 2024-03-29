/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(thingy91_golioth, LOG_LEVEL_DBG);

#include <modem/lte_lc.h>
#include <net/golioth/system_client.h>
#include <samples/common/net_connect.h>
#include <zephyr/net/coap.h>
#include "app_rpc.h"
#include "app_settings.h"
#include "app_state.h"
#include "app_work.h"
#include "dfu/app_dfu.h"

#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_MODEM_INFO
#include <modem/modem_info.h>
#endif

static struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

K_SEM_DEFINE(connected, 0, 1);
K_SEM_DEFINE(dfu_status_unreported, 1, 1);

static k_tid_t _system_thread;

static const struct gpio_dt_spec golioth_led = GPIO_DT_SPEC_GET(DT_ALIAS(golioth_led), gpios);
static const struct gpio_dt_spec user_btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static struct gpio_callback button_cb_data;

/* forward declarations */
void golioth_connection_led_set(uint8_t state);

void wake_system_thread(void)
{
	k_wakeup(_system_thread);
}

static void golioth_on_connect(struct golioth_client *client)
{
	k_sem_give(&connected);
	golioth_connection_led_set(1);

	LOG_INF("Registering observations with Golioth");
	app_dfu_observe();
	app_settings_observe();
	app_rpc_observe();
	app_state_observe();

	if (k_sem_take(&dfu_status_unreported, K_NO_WAIT) == 0) {
		/* Report firmware update status on first connect after power up */
		app_dfu_report_state_to_golioth();
	}
}

#ifdef CONFIG_SOC_NRF9160
static void process_lte_connected(void)
{
	golioth_system_client_start();
}

/**
 * @brief Perform actions based on LTE connection events
 *
 * This is copied from the Golioth samples/common/nrf91_lte_monitor.c to allow us to perform custom
 * actions (turn on LED and start Golioth client) when a network connection becomes available.
 *
 * Set `CONFIG_GOLIOTH_SAMPLE_NRF91_LTE_MONITOR=n` so that the common sample code doesn't collide.
 *
 * @param evt
 */
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		switch (evt->nw_reg_status) {
		case LTE_LC_NW_REG_NOT_REGISTERED:
			LOG_INF("Network: Not registered");
			break;
		case LTE_LC_NW_REG_REGISTERED_HOME:
			LOG_INF("Network: Registered (home)");
			process_lte_connected();
			break;
		case LTE_LC_NW_REG_SEARCHING:
			LOG_INF("Network: Searching");
			break;
		case LTE_LC_NW_REG_REGISTRATION_DENIED:
			LOG_INF("Network: Registration denied");
			break;
		case LTE_LC_NW_REG_UNKNOWN:
			LOG_INF("Network: Unknown");
			break;
		case LTE_LC_NW_REG_REGISTERED_ROAMING:
			LOG_INF("Network: Registered (roaming)");
			process_lte_connected();
			break;
		case LTE_LC_NW_REG_UICC_FAIL:
			LOG_INF("Network: UICC fail");
			break;
		}
		break;
	case LTE_LC_EVT_RRC_UPDATE:
		switch (evt->rrc_mode) {
		case LTE_LC_RRC_MODE_CONNECTED:
			LOG_DBG("RRC: Connected");
			break;
		case LTE_LC_RRC_MODE_IDLE:
			LOG_DBG("RRC: Idle");
			break;
		}
		break;
	default:
		break;
	}
}
#endif /* CONFIG_SOC_NRF9160 */

#ifdef CONFIG_MODEM_INFO
static void log_modem_firmware_version(void)
{
	char sbuf[128];

	/* Initialize modem info */
	int err = modem_info_init();

	if (err) {
		LOG_ERR("Failed to initialize modem info: %d", err);
	}

	/* Log modem firmware version */
	modem_info_string_get(MODEM_INFO_FW_VERSION, sbuf, sizeof(sbuf));
	LOG_INF("Modem firmware version: %s", sbuf);
}
#endif

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	uint32_t button_kernel_time = k_cycle_get_32();

	LOG_DBG("Button pressed at %d", button_kernel_time);

	play_beep_once();
	k_wakeup(_system_thread);
}

/* Set (unset) LED indicators for active Golioth connection */
void golioth_connection_led_set(uint8_t state)
{
	app_led_pwm_init(); /* Once the connection is available, fire up the LED pwms */
}

int main(void)
{
	int err;

	LOG_DBG("Start Thingy91 Golioth sample");

	LOG_INF("Firmware version: %s", CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION);
	IF_ENABLED(CONFIG_MODEM_INFO, (log_modem_firmware_version();));

	/* Get system thread id so loop delay change event can wake main */
	_system_thread = k_current_get();

	/* Initialize Golioth logo LED */
	err = gpio_pin_configure_dt(&golioth_led, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Unable to configure LED for Golioth Logo");
	}

	/* Initialize app state */
	app_state_init(client);

	/* Initialize app work */
	app_work_init(client);

	/* Initialize DFU components */
	app_dfu_init(client);

	/* Initialize app settings */
	app_settings_init(client);

	/* Initialize app RPC */
	app_rpc_init(client);

	/* Register Golioth on_connect callback */
	client->on_connect = golioth_on_connect;

	/* Start LTE asynchronously if the nRF9160 is used
	 * Golioth Client will start automatically when LTE connects
	 */
	IF_ENABLED(CONFIG_SOC_NRF9160, (LOG_INF("Connecting to LTE, this may take some time...");
					lte_lc_init_and_connect_async(lte_handler);));

	/* If nRF9160 is not used, start the Golioth Client and block until connected */
	if (!IS_ENABLED(CONFIG_SOC_NRF9160)) {
		/* Run WiFi/DHCP if necessary */
		if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLES_COMMON)) {
			net_connect();
		}

		/* Start Golioth client */
		golioth_system_client_start();

		/* Block until connected to Golioth */
		k_sem_take(&connected, K_FOREVER);
	}

	err = app_buzzer_init();
	if (err) {
		LOG_ERR("Unable to configure buzzer");
	}

	/* Set up user button */
	err = gpio_pin_configure_dt(&user_btn, GPIO_INPUT);
	if (err) {
		LOG_ERR("Error %d: failed to configure %s pin %d", err, user_btn.port->name,
			user_btn.pin);
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&user_btn, GPIO_INT_EDGE_TO_ACTIVE);
	if (err) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", err,
			user_btn.port->name, user_btn.pin);
		return err;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(user_btn.pin));
	gpio_add_callback(user_btn.port, &button_cb_data);

	while (true) {
		app_work_sensor_read();
		state_counter_change();

		k_sleep(K_SECONDS(get_loop_delay_s()));
	}
}
