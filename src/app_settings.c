/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <sys/_stdint.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/settings.h>
#include <zephyr/drivers/pwm.h>
#include "main.h"

#include <zephyr/kernel.h>

#include "app_settings.h"

int period = 100000; /* should be 100 uSec */

static int32_t _loop_delay_s = 60;
#define LOOP_DELAY_S_MAX 43200
#define LOOP_DELAY_S_MIN 1
#define LED_FADE_SPEED_MS_MAX 10000
#define LED_FADE_SPEED_MS_MIN 500

enum LED_PCT_CB_INDEX {
	LED_R_CB_ARG,
	LED_G_CB_ARG,
	LED_B_CB_ARG,
};

static int32_t _red_intensity_pct = 50;
static int32_t _green_intensity_pct = 50;
static int32_t _blue_intensity_pct = 50;

static int32_t _led_fade_speed_ms = 1200;

static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));
static const struct pwm_dt_spec pwm_led1 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1));
static const struct pwm_dt_spec pwm_led2 = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2));

K_SEM_DEFINE(led_pwm_initialized_sem, 0, 1); /* Wait until led pwm is ready */

#define LED_PWM_STACK 4096

static int led_on_off = 1;

float intensity_steps[] = {1.00, 0.95, 0.90, 0.85, 0.80, 0.75, 0.70, 0.65, 0.60, 0.55,
			   0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95};
int array_size = sizeof(intensity_steps) / sizeof(float);

void all_leds_on(void)
{
	led_on_off = 1;
}

void all_leds_off(void)
{
	led_on_off = 0;
}

extern void led_pwm_thread(void *d0, void *d1, void *d2)
{
	/* Block until led pwm is ready */
	k_sem_take(&led_pwm_initialized_sem, K_FOREVER);
	while (1) {
		int ret = 0;
		float pulse_ns = 0;

		for (int i = 0; i < array_size; i++) {
			pulse_ns = (((float)period * (float)_red_intensity_pct *
				     intensity_steps[i] * (float)led_on_off) /
				    100);
			ret = pwm_set_dt(&pwm_led0, period, (int)pulse_ns);
			pulse_ns = (((float)period * (float)_green_intensity_pct *
				     intensity_steps[i] * (float)led_on_off) /
				    100);
			ret = pwm_set_dt(&pwm_led1, period, (int)pulse_ns);
			pulse_ns = (((float)period * (float)_blue_intensity_pct *
				     intensity_steps[i] * (float)led_on_off) /
				    100);
			ret = pwm_set_dt(&pwm_led2, period, (int)pulse_ns);
			/* Sleep thread until next increment of pulsing effect */
			k_sleep(K_MSEC(_led_fade_speed_ms / array_size));
		}
	}
}

int app_led_pwm_init(void)
{
	LOG_DBG("turning on pwm leds");
	k_sem_give(&led_pwm_initialized_sem);
	return 0;
}

K_THREAD_DEFINE(led_pwm_tid, LED_PWM_STACK, led_pwm_thread, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

int32_t get_loop_delay_s(void)
{
	return _loop_delay_s;
}

static enum golioth_settings_status on_loop_delay_setting(int32_t new_value, void *arg)
{
	/* Only update if value has changed */
	if (_loop_delay_s == new_value) {
		LOG_DBG("Received LOOP_DELAY_S already matches local value.");
	} else {
		_loop_delay_s = new_value;
		LOG_INF("Set loop delay to %i seconds", _loop_delay_s);

		wake_system_thread();
	}
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_fade_speed_setting(int32_t new_value, void *arg)
{
	/* Only update if value has changed */
	if (_led_fade_speed_ms == new_value) {
		LOG_DBG("Received LED_FADE_SPEED_MS already matches local value.");
	}

	else {
		_led_fade_speed_ms = new_value;
		LOG_INF("Set LED fade speed to %d milliseconds", _led_fade_speed_ms);
		/* not waking system thread here, since the LED update thread is looping */
	}
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_led_pct_setting(int32_t new_value, void *arg)
{
	int32_t *global_intensity_pct;
	char color_letter;

	switch ((int) arg) {
		case LED_R_CB_ARG:
			global_intensity_pct = &_red_intensity_pct;
			color_letter = 'R';
			break;
		case LED_G_CB_ARG:
			global_intensity_pct = &_green_intensity_pct;
			color_letter = 'G';
			break;
		case LED_B_CB_ARG:
			global_intensity_pct = &_blue_intensity_pct;
			color_letter = 'B';
			break;
		default:
			LOG_ERR("Unexpected LED intensity index value: %i", (int) arg);
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
	}

	/* Only update if value has changed */
	if (*global_intensity_pct == new_value) {
		LOG_DBG("Received %c intensity already matches local value.", color_letter);
	} else {
		*global_intensity_pct = new_value;
		LOG_INF("Set %c intensity to %d percent", color_letter, *global_intensity_pct);
		/* not waking system thread here, since the LED update thread is looping */
	}

	return GOLIOTH_SETTINGS_SUCCESS;
}

void check_register_settings_error_and_log(int err, const char *settings_str)
{
	if (err == 0)
	{
		return;
	}


	LOG_ERR("Failed to register settings callback for %s: %d", settings_str, err);
}

void app_settings_register(struct golioth_client *client)
{
	struct golioth_settings *settings = golioth_settings_init(client);
	int err;

	err = golioth_settings_register_int_with_range(settings,
							   "LOOP_DELAY_S",
							   LOOP_DELAY_S_MIN,
							   LOOP_DELAY_S_MAX,
							   on_loop_delay_setting,
							   NULL);

	check_register_settings_error_and_log(err, "LOOP_DELAY_S");

	err = golioth_settings_register_int_with_range(settings,
							   "LED_FADE_SPEED_MS",
							   LED_FADE_SPEED_MS_MIN,
							   LED_FADE_SPEED_MS_MAX,
							   on_fade_speed_setting,
							   NULL);

	check_register_settings_error_and_log(err, "LED_FADE_SPEED_MS");

	err = golioth_settings_register_int_with_range(settings,
							   "RED_INTENSITY_PCT",
							   0,
							   100,
							   on_led_pct_setting,
							   (void *) LED_R_CB_ARG);

	check_register_settings_error_and_log(err, "RED_INTENSITY_PCT");

	err = golioth_settings_register_int_with_range(settings,
							   "GREEN_INTENSITY_PCT",
							   0,
							   100,
							   on_led_pct_setting,
							   (void *) LED_G_CB_ARG);

	check_register_settings_error_and_log(err, "GREEN_INTENSITY_PCT");

	err = golioth_settings_register_int_with_range(settings,
							   "BLUE_INTENSITY_PCT",
							   0,
							   100,
							   on_led_pct_setting,
							   (void *) LED_B_CB_ARG);

	check_register_settings_error_and_log(err, "BLUE_INTENSITY_PCT");
}
