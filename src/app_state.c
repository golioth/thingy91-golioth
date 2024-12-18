/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_state, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <zephyr/kernel.h>

#include "app_state.h"

#define DEVICE_STATE_DEFAULT "-1"
#define DEVICE_STATE_FMT "{\"counter_up\":%d,\"counter_down\":%d}"
#define MAX_COUNT	 10000
#define MIN_COUNT	 0

static int app_state_update_actual(void);

int32_t _counter_up = MIN_COUNT;
int32_t _counter_down = MAX_COUNT - 1;

enum counter_dir {
	COUNTER_UP,
	COUNTER_DN,
	COUNTER_DIR_TOTAL

};

#define APP_STATE_DESIRED_UP_ENDP "desired/counter_up"
#define APP_STATE_DESIRED_DN_ENDP "desired/counter_down"
#define APP_STATE_ACTUAL_ENDP	  "state"

const char *endp_list[COUNTER_DIR_TOTAL] = {
	APP_STATE_DESIRED_UP_ENDP,
	APP_STATE_DESIRED_DN_ENDP
};

static struct golioth_client *client;

static void async_handler(struct golioth_client *client,
			  enum golioth_status status,
			  const struct golioth_coap_rsp_code *coap_rsp_code,
			  const char *path,
			  void *arg)
{
	if (status != GOLIOTH_OK) {
		LOG_WRN("Failed to set state: %d", status);
		return;
	}

	LOG_DBG("State successfully set");
}

void state_counter_change(void)
{
	if (_counter_down > MIN_COUNT) {
		_counter_down--;
	} else {
		_counter_down = MAX_COUNT;
	}

	if (_counter_up < MAX_COUNT) {
		_counter_up++;
	} else {
		_counter_up = MIN_COUNT;
	}

	LOG_DBG("D: %d; U: %d", _counter_down, _counter_up);
	app_state_update_actual();
}

static int app_state_reset_desired(enum counter_dir counter_idx)
{
	if ((counter_idx < 0) || (counter_idx > COUNTER_DIR_TOTAL)) {
		LOG_ERR("Counter index out of bounds: %d", counter_idx);
		return -EFAULT;
	}

	LOG_INF("Resetting \"%s\" LightDB State path to default.", endp_list[counter_idx]);

	int err;
	err = golioth_lightdb_set_async(client,
					endp_list[counter_idx],
					GOLIOTH_CONTENT_TYPE_JSON,
					DEVICE_STATE_DEFAULT,
					strlen(DEVICE_STATE_DEFAULT),
					async_handler,
					NULL);
	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
	return err;
}

static int app_state_update_actual(void)
{

	char sbuf[sizeof(DEVICE_STATE_FMT) + 22]; /* space for two int32_t values */

	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, _counter_up, _counter_down);

	int err;

	err = golioth_lightdb_set_async(client,
					APP_STATE_ACTUAL_ENDP,
					GOLIOTH_CONTENT_TYPE_JSON,
					sbuf,
					strlen(sbuf),
					async_handler,
					NULL);

	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
	return err;
}

static void app_state_desired_handler(struct golioth_client *client, enum golioth_status status,
				      const struct golioth_coap_rsp_code *coap_rsp_code,
				      const char *path, const uint8_t *payload, size_t payload_size,
				      void *arg)
{
	int err = 0;
	int direction = (enum counter_dir)arg;
	int32_t *cur_counter;

	switch (direction) {
		case COUNTER_UP:
			cur_counter = &_counter_up;
			break;
		case COUNTER_DN:
			cur_counter = &_counter_down;
			break;
		default:
			LOG_ERR("User arg out of bounds: %d", direction);
			return;
	}

	const char *endp_str = endp_list[direction];

	if (status != GOLIOTH_OK) {
		LOG_ERR("Failed to receive '%s' endpoint: %d", endp_str, status);
		return;
	}

	LOG_HEXDUMP_DBG(payload, payload_size, endp_str);

	/* Smallest int32_t is -2,147,483,647 -> 11 digits */
	if (payload_size > 11)
	{
		LOG_ERR("Payload too large: %d", payload_size);
		goto reset_desired;
	}

	/* 11 digits for int32_t + 1 digit for null terminator */
	char payload_str[12] = {0};
	memcpy(payload_str, payload, MIN(sizeof(payload_str) - 1, payload_size));

	if (strcmp(DEVICE_STATE_DEFAULT, payload_str) == 0) {
		/* Received the default desired value, do nothing */
		return;
	}

	errno = 0;
	char *end;

	int32_t new_value = strtol(payload_str, &end, 10);
	if ((errno != 0) || (end == payload_str)) {
		LOG_ERR("Error converting string to number: %s", payload_str);
		goto reset_desired;
	}

	LOG_INF("Got payload for %s: %d", endp_str, new_value);

	if ((new_value < MIN_COUNT) || (new_value > MAX_COUNT)) {
		LOG_ERR("New %s value out of bounds: %d", endp_str, new_value);
		goto reset_desired;
	}

	if (new_value == *cur_counter) {
		LOG_INF("Stored value matches %s", endp_str);
		goto reset_desired;
	}

	*cur_counter = new_value;
	LOG_INF("Storing %d as new value for %s", new_value, endp_str);
	err = app_state_update_actual();
	if (err) {
		LOG_ERR("Failed to update cloud state: %d", err);
	}

reset_desired:
	app_state_reset_desired(direction);
	return;
}

int app_state_observe(struct golioth_client *state_client)
{
	int err;

	client = state_client;

	err = golioth_lightdb_observe_async(client,
					    APP_STATE_DESIRED_UP_ENDP,
					    GOLIOTH_CONTENT_TYPE_JSON,
					    app_state_desired_handler,
					    (void *) COUNTER_UP);
	if (err) {
		LOG_WRN("failed to observe lightdb path: %d", err);
		return err;
	}

	err = golioth_lightdb_observe_async(client,
					    APP_STATE_DESIRED_DN_ENDP,
					    GOLIOTH_CONTENT_TYPE_JSON,
					    app_state_desired_handler,
					    (void *) COUNTER_DN);
	if (err) {
		LOG_WRN("failed to observe lightdb path: %d", err);
		return err;
	}

	/* This will only run once. It updates the actual state of the device
	 * with the Golioth servers. Future updates will be sent whenever
	 * changes occur.
	 */
	err = app_state_update_actual();

	return err;
}
