/*
 * Copyright (c) 2022 Golioth, Inc.
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

static struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

K_SEM_DEFINE(connected, 0, 1);
K_SEM_DEFINE(dfu_status_update, 0, 1);

static k_tid_t _system_thread = 0;

static const struct gpio_dt_spec golioth_led = GPIO_DT_SPEC_GET(
		DT_ALIAS(golioth_led), gpios);
static const struct gpio_dt_spec user_btn = GPIO_DT_SPEC_GET(
		DT_ALIAS(sw1), gpios);
static struct gpio_callback button_cb_data;

/* forward declarations */
void golioth_connection_led_set(uint8_t state);
void network_led_set(uint8_t state);

void wake_system_thread(void) {
	k_wakeup(_system_thread);
}

static void golioth_on_connect(struct golioth_client *client)
{
	k_sem_give(&connected);

	LOG_INF("Registering observations with Golioth");
	app_dfu_observe();
	app_settings_observe();
	app_rpc_observe();
	app_state_observe();

	static bool initial_connection = true;
	if (initial_connection) {
		initial_connection = false;

		/* Report current DFU version to Golioth */
		app_dfu_report_state_to_golioth();

		/* Indicate connection using LEDs */
		// golioth_connection_led_set(1);
		app_led_pwm_init();
	}
}

static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
		 (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			break;
		}

		LOG_INF("Connected to LTE network");
		network_led_set(1);

		golioth_system_client_start();

		break;
	case LTE_LC_EVT_PSM_UPDATE:
	case LTE_LC_EVT_EDRX_UPDATE:
	case LTE_LC_EVT_RRC_UPDATE:
	case LTE_LC_EVT_CELL_UPDATE:
	case LTE_LC_EVT_LTE_MODE_UPDATE:
	case LTE_LC_EVT_TAU_PRE_WARNING:
	case LTE_LC_EVT_NEIGHBOR_CELL_MEAS:
	case LTE_LC_EVT_MODEM_SLEEP_EXIT_PRE_WARNING:
	case LTE_LC_EVT_MODEM_SLEEP_EXIT:
	case LTE_LC_EVT_MODEM_SLEEP_ENTER:
		/* Callback events carrying LTE link data */
		break;
	 default:
		break;
	}
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
					uint32_t pins)
{
	uint32_t button_kernel_time = k_cycle_get_32();
	LOG_DBG("Button pressed at %d", button_kernel_time);

	play_beep_once();
	k_wakeup(_system_thread);
}

/* Set (unset) LED indicators for active Golioth connection */
void golioth_connection_led_set(uint8_t state) {

	app_led_pwm_init(); // Once the connection is available, fire up the LED pwms

}

/* Set (unset) LED indicators for active internet connection */
void network_led_set(uint8_t state) {
	/* Currently unused */
}

void main(void)
{
	int err;

	LOG_DBG("Start Thingy91 Golioth sample");

	LOG_INF("Firmware version: %s", CONFIG_MCUBOOT_IMAGE_VERSION);

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

	/* Run WiFi/DHCP if necessary */
	if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLES_COMMON)) {
		net_connect();
	}

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		LOG_INF("Device is using automatic LTE control");
		network_led_set(1);

		/* Start Golioth client */
		golioth_system_client_start();

		/* Block until connected to Golioth */
		k_sem_take(&connected, K_FOREVER);

	} else if (IS_ENABLED(CONFIG_SOC_NRF9160)){
		LOG_INF("Connecting to LTE network. This may take a few minutes...");
		err = lte_lc_init_and_connect_async(lte_handler);
		if (err) {
			 printk("lte_lc_init_and_connect_async, error: %d\n", err);
			 return;
		}
	}

	err = app_buzzer_init();
	if (err) {
		LOG_ERR("Unable to configure buzzer");
	}


	/* Set up user button */
	err = gpio_pin_configure_dt(&user_btn, GPIO_INPUT);
	if (err != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
				err, user_btn.port->name, user_btn.pin);
		return;
	}

	err = gpio_pin_interrupt_configure_dt(&user_btn,
	                                      GPIO_INT_EDGE_TO_ACTIVE);
	if (err != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
				err, user_btn.port->name, user_btn.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(user_btn.pin));
	gpio_add_callback(user_btn.port, &button_cb_data);

	while (true) {
		if (k_sem_take(&dfu_status_update, K_NO_WAIT) == 0) {
			app_dfu_report_state_to_golioth();
		}

		app_work_sensor_read();
		state_counter_change();

		k_sleep(K_SECONDS(get_loop_delay_s()));

	}
}
