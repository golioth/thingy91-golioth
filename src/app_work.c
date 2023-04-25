/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
// #include <zephyr/drivers/led.h>


#include "app_work.h"
#include "libostentus/libostentus.h"

#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

// // From Jared's video about PWM

// #define PWM_BUZZER_NODE DT_NODELABEL(pwm_buzzer0)
// #define PWM_CTRL DT_PWMS_CTLR(PWM_BUZZER_NODE)
// #define PWM_CHAN DT_PWMS_CHANNEL(PWM_BUZZER_NODE)
// #define PWM_FLAGS DT_PWMS_FLAGS (PWM_BUZZER_NODE)

static struct golioth_client *client;



/* Sensor device structs */
const struct device *light = DEVICE_DT_GET_ONE(rohm_bh1749);
const struct device *weather = DEVICE_DT_GET_ONE(bosch_bme680);

const struct pwm_dt_spec sBuzzer = PWM_DT_SPEC_GET(DT_ALIAS(buzzer_pwm));


static bool sBuzzerState;
int freq = 440;

// const struct device *buzzer_pwm_dev = DEVICE_DT_GET(PWM_CTRL);

// if (buzzer_pwm_dev == NULL||!device_is_ready(buzzer_pwm_dev))
// {
// 	LOG_ERR("Error: PWM device is not ready");
// }




/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT	"{\"counter\":%d}"

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

void BuzzerSetState(bool onOff)
{
	sBuzzerState = onOff;
	pwm_set_pulse_dt(&sBuzzer, sBuzzerState ? (sBuzzer.period / 2) : 0);
}

void BuzzerToggleState()
{
	BuzzerSetState(!sBuzzerState);
}

void BuzzerSetFreq(int freq)
{
	if (freq < BUZZER_MAX_FREQ && freq > BUZZER_MIN_FREQ)
	{
		pwm_set_dt(&sBuzzer,PWM_HZ(freq),PWM_HZ(freq)/2);
		LOG_DBG("Frequency set to %d Hz", freq);
	}
	else
	{
		LOG_ERR("Frequency %d is outside allowable range", freq);
	}	
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	char json_buf[256];

	// // PWM variables

	// uint32_t max_period = MAX_PERIOD;
	// uint32_t period;
	// uint8_t dir = 0U;

	// period = max_period;

	// Sensor value structs

	struct sensor_value BH1749_RED;
	struct sensor_value BH1749_GREEN;
	struct sensor_value BH1749_BLUE;
	struct sensor_value BH1749_IR;
	struct sensor_value temp;
	struct sensor_value press;
	struct sensor_value humidity;
	struct sensor_value gas_res;



	/* Log battery levels if possible */
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (log_battery_info();));

	/* For this demo, we just send Hello to Golioth */
	static uint8_t counter;


	// err = pwm_pin_set_usec(buzzer_pwm_dev, PWM_CHAN, LED_PWM_PERIOD_US, pulse, PWM_FLAGS);
	// if (err)
	// {
	// 	LOG_ERR("Pwm set fail. Err: %i", err);	
	// }

	BuzzerSetFreq(freq);
	freq+=40;

	err = sensor_sample_fetch_chan(light, SENSOR_CHAN_ALL);
	/* The sensor does only support fetching SENSOR_CHAN_ALL */
	if (err) {
		printk("sensor_sample_fetch failed err %d\n", err);
		return;
	}

	err = sensor_channel_get(light, SENSOR_CHAN_RED, &BH1749_RED);
	if (err) {
		printk("sensor_channel_get failed err %d\n", err);
		return;
	}
	printk("BH1749 RED: %d\n", BH1749_RED.val1);

	err = sensor_channel_get(light, SENSOR_CHAN_GREEN, &BH1749_GREEN);
	if (err) {
		printk("sensor_channel_get failed err %d\n", err);
		return;
	}
	printk("BH1749 GREEN: %d\n", BH1749_GREEN.val1);

	err = sensor_channel_get(light, SENSOR_CHAN_BLUE, &BH1749_BLUE);
	if (err) {
		printk("sensor_channel_get failed err %d\n", err);
		return;
	}
	printk("BH1749 BLUE: %d\n", BH1749_BLUE.val1);

	err = sensor_channel_get(light, SENSOR_CHAN_IR, &BH1749_IR);
	if (err) {
		printk("sensor_channel_get failed err %d\n", err);
		return;
	}
	printk("BH1749 IR: %d\n", BH1749_IR.val1);

	// BME680 

	sensor_sample_fetch(weather);
	sensor_channel_get(weather, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(weather, SENSOR_CHAN_PRESS, &press);
	sensor_channel_get(weather, SENSOR_CHAN_HUMIDITY, &humidity);
	sensor_channel_get(weather, SENSOR_CHAN_GAS_RES, &gas_res);


	printk("T: %d.%06d; P: %d.%06d; H: %d.%06d; G: %d.%06d\n",
				temp.val1, temp.val2, press.val1, press.val2,
				humidity.val1, humidity.val2, gas_res.val1,
				gas_res.val2);




	LOG_INF("Sending hello! %d", counter);

	err = golioth_send_hello(client);
	if (err) {
		LOG_WRN("Failed to send hello!");
	}

	/* Send sensor data to Golioth */
	/* For this demo we just fake it */
	snprintk(json_buf, sizeof(json_buf), JSON_FMT, counter);

	err = golioth_stream_push_cb(client, "sensor",
			GOLIOTH_CONTENT_FORMAT_APP_JSON,
			json_buf, strlen(json_buf),
			async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	/* Update slide values on Ostentus
	 *  -values should be sent as strings
	 *  -use the enum from app_work.h for slide key values
	 */
	snprintk(json_buf, 6, "%d", counter);
	slide_set(UP_COUNTER, json_buf, strlen(json_buf));
	snprintk(json_buf, 6, "%d", 255-counter);
	slide_set(DN_COUNTER, json_buf, strlen(json_buf));

	++counter;
}

void app_work_init(struct golioth_client *work_client)
{
	client = work_client;
	BuzzerSetState(true);
}

