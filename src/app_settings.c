/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <net/golioth/settings.h>
#include <zephyr/drivers/pwm.h>
#include "main.h"

#include <zephyr/kernel.h>

#include "app_settings.h"

int period = 100000;  // should be 100 uSec

static struct golioth_client *client;

static int32_t _loop_delay_s = 60;

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

float intensity_steps[]={1.00,0.95,0.90,0.85,0.80,0.75,0.70,0.65,0.60,0.55,0.50,0.55,0.60,0.65,0.70,0.75,0.80,0.85,0.90,0.95};
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
	/* Block until buzzer is available */
	k_sem_take(&led_pwm_initialized_sem, K_FOREVER);
	while (1) {
		int ret = 0;
		float pulse_ns = 0;
		
		for (int i=0;i<array_size;i++)
		{
			pulse_ns = ( ( (float)period * (float)_red_intensity_pct * intensity_steps[i] * (float)led_on_off) / 100 );
			ret = pwm_set_dt(&pwm_led0, period, (int)pulse_ns);
			pulse_ns = ( ( (float)period * (float)_green_intensity_pct * intensity_steps[i] * (float)led_on_off) / 100 );
			ret = pwm_set_dt(&pwm_led1, period, (int)pulse_ns);
			pulse_ns = ( ( (float)period * (float)_blue_intensity_pct * intensity_steps[i] * (float)led_on_off) / 100 );
			ret = pwm_set_dt(&pwm_led2, period, (int)pulse_ns);
			k_sleep(K_MSEC(_led_fade_speed_ms/array_size));	// Sleep thread until next increment of pulsing effect
		}
	}
}

int app_led_pwm_init()
{

	LOG_DBG("turning on pwm leds");
	k_sem_give(&led_pwm_initialized_sem);
	return 0;
}

K_THREAD_DEFINE(led_pwm_tid, LED_PWM_STACK,
		led_pwm_thread, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

int32_t get_loop_delay_s(void)
{
	return _loop_delay_s;
}

enum golioth_settings_status on_setting(const char *key, const struct golioth_settings_value *value)
{

	LOG_DBG("Received setting: key = %s, type = %d", key, value->type);
	if (strcmp(key, "LOOP_DELAY_S") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received LOOP_DELAY_S is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [1, 43200] */
		if (value->i64 < 1 || value->i64 > 43200) {
			LOG_DBG("Received LOOP_DELAY_S setting is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_loop_delay_s == (int32_t)value->i64) {
			LOG_DBG("Received LOOP_DELAY_S already matches local value.");
		} else {
			_loop_delay_s = (int32_t)value->i64;
			LOG_INF("Set loop delay to %d seconds", _loop_delay_s);

			wake_system_thread();
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	LOG_DBG("Received setting: key = %s, type = %d", key, value->type);
	if (strcmp(key, "LED_FADE_SPEED_MS") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received LED_FADE_SPEED_MS is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [1, 43200] */
		if (value->i64 < 500 || value->i64 > 10000) {
			LOG_DBG("Received LED_FADE_SPEED_MS setting is outside allowed range (500-10000 ms).");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_led_fade_speed_ms == (int32_t)value->i64) {
			LOG_DBG("Received LED_FADE_SPEED_MS already matches local value.");
		}
		
		else {
			_led_fade_speed_ms = (int32_t)value->i64;
			LOG_INF("Set LED fade speed to %d milliseconds", _led_fade_speed_ms);
			/* not waking system thread here, since the LED update thread is looping */
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "RED_INTENSITY_PCT") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received RED_INTENSITY_PCT is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [1, 43200] */
		if (value->i64 < 0 || value->i64 > 100) {
			LOG_DBG("Received RED_INTENSITY_PCT setting is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_red_intensity_pct == (int32_t)value->i64) {
			LOG_DBG("Received RED_INTENSITY_PCT already matches local value.");
		}
		else {
			_red_intensity_pct = (int32_t)value->i64;
			LOG_INF("Set red intensity to %d percent", _red_intensity_pct);
			/* not waking system thread here, since the LED update thread is looping */
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}



	if (strcmp(key, "GREEN_INTENSITY_PCT") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received GREEN_INTENSITY_PCT is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [1, 43200] */
		if (value->i64 < 0 || value->i64 > 100) {
			LOG_DBG("Received GREEN_INTENSITY_PCT setting is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_green_intensity_pct == (int32_t)value->i64) {
			LOG_DBG("Received GREEN_INTENSITY_PCT already matches local value.");
		}
		else {
			_green_intensity_pct = (int32_t)value->i64;
			LOG_INF("Set green intensity to %d percent", _green_intensity_pct);
			/* not waking system thread here, since the LED update thread is looping */
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "BLUE_INTENSITY_PCT") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received BLUE_INTENSITY_PCT is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [1, 43200] */
		if (value->i64 < 0 || value->i64 > 100) {
			LOG_DBG("Received BLUE_INTENSITY_PCT setting is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_blue_intensity_pct == (int32_t)value->i64) {
			LOG_DBG("Received BLUE_INTENSITY_PCT already matches local value.");
		}
		else {
			_blue_intensity_pct = (int32_t)value->i64;
			LOG_INF("Set blue intensity to %d percent", _blue_intensity_pct);
			/* not waking system thread here, since the LED update thread is looping */
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	/* If the setting is not recognized, we should return an error */
	return GOLIOTH_SETTINGS_KEY_NOT_RECOGNIZED;
}

int app_settings_init(struct golioth_client *state_client)
{
	client = state_client;
	int err = app_settings_register(client);
	return err;
}

int app_settings_observe(void)
{
	int err = golioth_settings_observe(client);
	if (err) {
		LOG_ERR("Failed to observe settings: %d", err);
	}
	return err;
}

int app_settings_register(struct golioth_client *settings_client)
{
	int err = golioth_settings_register_callback(settings_client, on_setting);

	if (err) {
		LOG_ERR("Failed to register settings callback: %d", err);
	}

	return err;
}
