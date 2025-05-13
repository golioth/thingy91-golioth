/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(thingy91_golioth, LOG_LEVEL_DBG);

#include <app_version.h>
#include "app_buzzer.h"
#include "app_rpc.h"
#include "app_settings.h"
#include "app_state.h"
#include "app_sensors.h"
#include <golioth/client.h>
#include <golioth/stream.h>
#include <golioth/fw_update.h>
#include <samples/common/net_connect.h>
#include <samples/common/sample_credentials.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_SOC_SERIES_NRF91X
#include <modem/lte_lc.h>
#endif

#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_MODEM_INFO
#include <modem/modem_info.h>
#endif

/* Current firmware version; update in VERSION */
static const char *_current_version =
	STRINGIFY(APP_VERSION_MAJOR) "." STRINGIFY(APP_VERSION_MINOR) "." STRINGIFY(APP_PATCHLEVEL);

static struct golioth_client *client;
K_SEM_DEFINE(connected, 0, 1);

static k_tid_t _system_thread;

static const struct gpio_dt_spec user_btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static struct gpio_callback button_cb_data;

void wake_system_thread(void)
{
	k_wakeup(_system_thread);
}

static void on_client_event(struct golioth_client *client, enum golioth_client_event event,
			    void *arg)
{
	bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);

	if (is_connected) {
		k_sem_give(&connected);
		app_led_pwm_init();
	}
	LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void start_golioth_client(void)
{
	/* Get the client configuration from auto-loaded settings */
	const struct golioth_client_config *client_config = golioth_sample_credentials_get();

	/* Create and start a Golioth Client */
	client = golioth_client_create(client_config);

	/* Register Golioth on_connect callback */
	golioth_client_register_event_callback(client, on_client_event, NULL);

	/* Initialize DFU components */
	golioth_fw_update_init(client, _current_version);

	/*** Call Golioth APIs for other services in dedicated app files ***/

	/* Observe State service data */
	app_state_observe(client);

	/* Set Golioth Client for streaming sensor data */
	app_sensors_set_client(client);

	/* Register Settings service */
	app_settings_register(client);

	/* Register RPC service */
	app_rpc_register(client);
}

#ifdef CONFIG_SOC_SERIES_NRF91X

static void lte_handler(const struct lte_lc_evt *const evt)
{
	if (evt->type == LTE_LC_EVT_NW_REG_STATUS) {

		if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) ||
		    (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) {

			/* Change the state of the Internet LED on Ostentus */
			IF_ENABLED(CONFIG_LIB_OSTENTUS, (ostentus_led_internet_set(o_dev, 1);));

			if (!client) {
				/* Create and start a Golioth Client */
				start_golioth_client();
			}
		}
	}
}

#endif /* CONFIG_SOC_SERIES_NRF91X */

#ifdef CONFIG_MODEM_INFO
static void log_modem_firmware_version(void)
{
	char sbuf[128];

	/* Initialize modem info */
	int err = modem_info_init();

	if (err) {
		LOG_ERR("Failed to initialize modem info: %d", err);
		return;
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

int main(void)
{
	int err;

	LOG_DBG("Start Thingy91 Golioth sample");

	LOG_INF("Firmware version: %s", _current_version);
	IF_ENABLED(CONFIG_MODEM_INFO, (log_modem_firmware_version();));

	/* Get system thread id so loop delay change event can wake main */
	_system_thread = k_current_get();

#ifdef CONFIG_SOC_SERIES_NRF91X
	/* Start LTE asynchronously if the nRF9160 is used.
	 * Golioth Client will start automatically when LTE connects
	 */

	LOG_INF("Connecting to LTE, this may take some time...");
	lte_lc_connect_async(lte_handler);

#else
	/* If nRF9160 is not used, start the Golioth Client and block until connected */

	/* Start Golioth client */
	golioth_system_client_start();

	/* Block until connected to Golioth */
	k_sem_take(&connected, K_FOREVER);

	/* Start Golioth client */
	start_golioth_client();

	/* Block until connected to Golioth */
	k_sem_take(&connected, K_FOREVER);
#endif /* CONFIG_SOC_SERIES_NRF91X */

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
		app_sensors_read_and_stream();
		app_state_counter_change();

		k_sleep(K_SECONDS(get_loop_delay_s()));
	}
}
