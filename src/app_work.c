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

static struct golioth_client *client;

static const struct gpio_dt_spec user_led = GPIO_DT_SPEC_GET(
		DT_ALIAS(user_led), gpios);

/* Sensor device structs */
const struct device *light = DEVICE_DT_GET_ONE(rohm_bh1749);
const struct device *weather = DEVICE_DT_GET_ONE(bosch_bme680);

const struct pwm_dt_spec sBuzzer = PWM_DT_SPEC_GET(DT_ALIAS(buzzer_pwm));

int freq = 880;
typedef enum 
	{
		beep,
		funky_town,
		other_song	
	}song_choice;

song_choice song = 0;

/* Thread reads plays song on buzzer */

K_SEM_DEFINE(buzzer_initialized_sem, 0, 1); /* Wait until buzzer is ready */

#define BUZZER_STACK 1024

extern void buzzer_thread(void *d0, void *d1, void *d2) {
	/* Block until buzzer is available */
	k_sem_take(&buzzer_initialized_sem, K_FOREVER);
	while(1) {
		//LOG_DBG("Playing song from buzzer");
		// play song

		switch(song)
		{
			case 0:
				LOG_DBG("playing beep");
				pwm_set_dt(&sBuzzer,PWM_HZ(freq),PWM_HZ(freq)/2);
				k_msleep(300);
				break;
			case 1:
				LOG_DBG("playing funky town");
				pwm_set_dt(&sBuzzer,PWM_HZ(freq*2),PWM_HZ(freq)/2);
				k_msleep(200);
				pwm_set_dt(&sBuzzer,PWM_HZ(freq*2),PWM_HZ(freq)/2);
				k_msleep(200);
				break;
			default:
				LOG_WRN("invalid switch state");
				break;
		}

		// turn buzzer off (pulse duty to 0)
		pwm_set_pulse_dt(&sBuzzer, 0);

		// Sleepy time!
		//LOG_DBG("Putting song thread to sleep until woken");
		k_sleep(K_FOREVER);
	}
}

int app_buzzer_init()
{
	if (!device_is_ready(sBuzzer.dev)) {
		return -ENODEV;
	}
	k_sem_give(&buzzer_initialized_sem);
	return 0;
}

K_THREAD_DEFINE(buzzer_tid, BUZZER_STACK,
            buzzer_thread, NULL, NULL, NULL,
            K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);



void play_beep_once(void)
{
	song = beep;
	k_wakeup(buzzer_tid);
}

void play_funkytown_once(void)
{
	song = funky_town;
	k_wakeup(buzzer_tid);
}


/* Set (unset) LED indicators for user action */
void user_led_set(uint8_t state) {
	uint8_t pin_state = state ? 1 : 0;
	/* Turn on Golioth logo LED once connected */
	gpio_pin_set_dt(&user_led, pin_state);
	/* Change the state of the Golioth LED on Ostentus */
	led_user_set(pin_state);
}


/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT "{\
\"weather\":{\
	\"tem\":%d.%d,\
	\"pre\":%d.%d,\
	\"hum\":%d.%d,\
	\"gas\":%d.%d\
},\
\"light\":{\
	\"red\":%d,\
	\"blue\":%d,\
	\"green\":%d,\
	\"ir\":%d}\
}"

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

/* This will be called by the main() loop after delays or on button presses*/
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	char json_buf[256];

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

	// BH1749

	err = sensor_sample_fetch_chan(light, SENSOR_CHAN_ALL);
	/* The sensor does only support fetching SENSOR_CHAN_ALL */
	if (err) {
		LOG_ERR("sensor_sample_fetch failed err %d", err);
		return;
	}
	sensor_channel_get(light, SENSOR_CHAN_RED, &BH1749_RED);
	sensor_channel_get(light, SENSOR_CHAN_GREEN, &BH1749_GREEN);
	sensor_channel_get(light, SENSOR_CHAN_BLUE, &BH1749_BLUE);
	sensor_channel_get(light, SENSOR_CHAN_IR, &BH1749_IR);
	LOG_DBG("BH1749 R: %d, G: %d, B: %d, IR: %d", BH1749_RED.val1,BH1749_GREEN.val1,BH1749_BLUE.val1,BH1749_IR.val1);

	// BME680 

	sensor_sample_fetch(weather);
	sensor_channel_get(weather, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(weather, SENSOR_CHAN_PRESS, &press);
	sensor_channel_get(weather, SENSOR_CHAN_HUMIDITY, &humidity);
	sensor_channel_get(weather, SENSOR_CHAN_GAS_RES, &gas_res);


	LOG_DBG("T: %d.%06d; P: %d.%06d; H: %d.%06d; G: %d.%06d",
				temp.val1, temp.val2, press.val1, press.val2,
				humidity.val1, humidity.val2, gas_res.val1,
				gas_res.val2);


	// Format data for LightDB Stream

	snprintk(json_buf, sizeof(json_buf), JSON_FMT,
		temp.val1, temp.val2,
		press.val1, press.val2,
		humidity.val1, humidity.val2,
		gas_res.val1, gas_res.val2,
		BH1749_RED.val1,
		BH1749_GREEN.val1,
		BH1749_BLUE.val1,
		BH1749_IR.val1
		);

	// LOG_DBG("%s",json_buf);

	// Send to LightDB Stream on "sensor" endpoint

	err = golioth_stream_push_cb(client, "sensor",
			GOLIOTH_CONTENT_FORMAT_APP_JSON,
			json_buf, strlen(json_buf),
			async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	//play_funkytown_once();

	user_led_set(1);
	k_msleep(200);
	user_led_set(0);

}

void app_work_init(struct golioth_client *work_client)
{
	client = work_client;

}

