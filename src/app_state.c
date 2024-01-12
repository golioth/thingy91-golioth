/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_state, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/data/json.h>
#include "json_helper.h"

#include "app_state.h"
#include "app_work.h"

#define DEVICE_STATE_FMT "{\"counter_up\":%d,\"counter_down\":%d}"
#define MAX_COUNT	 10000
#define MIN_COUNT	 0

uint32_t _counter_up = MIN_COUNT;
uint32_t _counter_down = MAX_COUNT - 1;

static struct golioth_client *client;

static K_SEM_DEFINE(update_actual, 0, 1);

static int async_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_WRN("Failed to set state: %d", rsp->err);
		return rsp->err;
	}

	return 0;
}

void app_state_init(struct golioth_client *state_client)
{
	client = state_client;
	k_sem_give(&update_actual);
}


void state_counter_change(void)
{
	if (_counter_down > MIN_COUNT)
	{
		_counter_down--;
	}
	if (_counter_up < MAX_COUNT)
	{
		_counter_up++;
	}
	app_state_update_actual();
}

int app_state_reset_desired(void)
{
	LOG_INF("Resetting \"%s\" LightDB State endpoint to defaults.", APP_STATE_DESIRED_ENDP);

	char sbuf[sizeof(DEVICE_STATE_FMT) + 4]; /* space for two "-1" values */

	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, -1, -1);

	int err;

	err = golioth_lightdb_set_cb(client, APP_STATE_DESIRED_ENDP,
				     GOLIOTH_CONTENT_FORMAT_APP_JSON, sbuf, strlen(sbuf),
				     async_handler, NULL);
	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
	return err;
}

int app_state_update_actual(void)
{

	char sbuf[sizeof(DEVICE_STATE_FMT) + 20]; /* space for uint32 values */

	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, _counter_up, _counter_down);

	int err;

	err = golioth_lightdb_set_cb(client, APP_STATE_ACTUAL_ENDP, GOLIOTH_CONTENT_FORMAT_APP_JSON,
				     sbuf, strlen(sbuf), async_handler, NULL);

	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
	return err;
}

int app_state_desired_handler(struct golioth_req_rsp *rsp)
{
	int err = 0;
	int ret;

	if (rsp->err) {
		LOG_ERR("Failed to receive '%s' endpoint: %d", APP_STATE_DESIRED_ENDP, rsp->err);
		return rsp->err;
	}

	LOG_HEXDUMP_DBG(rsp->data, rsp->len, APP_STATE_DESIRED_ENDP);

	struct app_state parsed_state;

	ret = json_obj_parse((char *)rsp->data, rsp->len, app_state_descr,
			     ARRAY_SIZE(app_state_descr), &parsed_state);

	if (ret < 0) {
		LOG_ERR("Error parsing desired values: %d", ret);
		app_state_reset_desired();
		return ret;
	}

	uint8_t desired_processed_count = 0;
	uint8_t state_change_count = 0;

	if (ret & 1 << 0) {
		/* Process counter_up */
		if ((parsed_state.counter_up >= MIN_COUNT) &&
		    (parsed_state.counter_up < MAX_COUNT)) {
			LOG_DBG("Validated desired counter_up value: %d",
				parsed_state.counter_up);
			if (_counter_up != parsed_state.counter_up) {
				_counter_up = parsed_state.counter_up;
				++state_change_count;
			}
			++desired_processed_count;
		} else if (parsed_state.counter_up == -1) {
			LOG_DBG("No change requested for counter_up");
		} else {
			LOG_ERR("Invalid desired counter_up value: %d",
				parsed_state.counter_up);
			++desired_processed_count;
		}
	}
	if (ret & 1 << 1) {
		/* Process counter_down */
		if ((parsed_state.counter_down >= MIN_COUNT) &&
		    (parsed_state.counter_down < MAX_COUNT)) {
			LOG_DBG("Validated desired counter_down value: %d",
				parsed_state.counter_down);
			if (_counter_down != parsed_state.counter_down) {
				_counter_down = parsed_state.counter_down;
				++state_change_count;
			}
			++desired_processed_count;
			++state_change_count;
		} else if (parsed_state.counter_down == -1) {
			LOG_DBG("No change requested for counter_down");
		} else {
			LOG_ERR("Invalid desired counter_down value: %d",
				parsed_state.counter_down);
			++desired_processed_count;
		}
	}

	if (state_change_count) {
		/* The state was changed, so update the state on the Golioth servers */
		err = app_state_update_actual();
	}
	if (desired_processed_count) {
		/* We processed some desired changes to return these to -1 on the server
		 * to indicate the desired values were received.
		 */
		err = app_state_reset_desired();
	}

	return err;
}

int app_state_observe(void)
{
	int err = golioth_lightdb_observe_cb(client, APP_STATE_DESIRED_ENDP,
					     GOLIOTH_CONTENT_FORMAT_APP_JSON,
					     app_state_desired_handler, NULL);
	if (err) {
		LOG_WRN("failed to observe lightdb path: %d", err);
	}

	/* This will only run once. It updates the actual state of the device
	 * with the Golioth servers. Future updates will be sent whenever
	 * changes occur.
	 */
	if (k_sem_take(&update_actual, K_NO_WAIT) == 0) {
		err = app_state_update_actual();
	}

	return err;
}
