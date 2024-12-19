/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_state, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <zephyr/data/json.h>
#include <zephyr/kernel.h>

#define APP_STATE_DESIRED_PATH "desired"
#define APP_STATE_ACTUAL_PATH  "state"

K_MUTEX_DEFINE(counter_mutex);
#define COUNTER_UP_STRING "counter_up"
#define COUNTER_DN_STRING "counter_down"
#define COUNTER_MAX 9999
uint16_t _counter_up = COUNTER_MAX;
uint16_t _counter_dn = 0;
bool _initial_update_pending = true;

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

static bool encode_state(zcbor_state_t *zse, int32_t up, int32_t dn)
{
	bool ok = zcbor_map_start_encode(zse, 2) &&
	          zcbor_tstr_put_lit(zse, COUNTER_UP_STRING) &&
	          zcbor_int32_put(zse, up) &&
	          zcbor_tstr_put_lit(zse, COUNTER_DN_STRING) &&
	          zcbor_int32_put(zse, dn) &&
	          zcbor_map_end_encode(zse, 2);

	return ok;
}

int app_state_reset_desired(void)
{
	bool ok;
	char cbor_buf[64];
	ZCBOR_STATE_E(zse, 1, cbor_buf, sizeof(cbor_buf), 1);

	ok = encode_state(zse, -1, -1);

	if (!ok)
	{
		LOG_ERR("'%s' reset failed while try to encode CBOR.", APP_STATE_DESIRED_PATH);
		return -ENOMEM;
	}

	LOG_INF("Resetting \"%s\" LightDB State endpoint to defaults.", APP_STATE_DESIRED_PATH);

	size_t cbor_size = zse->payload - (const uint8_t *) cbor_buf;

	int err = golioth_lightdb_set_async(client,
					    APP_STATE_DESIRED_PATH,
					    GOLIOTH_CONTENT_TYPE_CBOR,
					    cbor_buf,
					    cbor_size,
					    async_handler,
					    NULL);
	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}

	return err;
}

int app_state_update_actual(void)
{
	bool ok;
	char cbor_buf[64];
	ZCBOR_STATE_E(zse, 1, cbor_buf, sizeof(cbor_buf), 1);

	k_mutex_lock(&counter_mutex, K_FOREVER);
	ok = encode_state(zse, (int32_t) _counter_up, (int32_t) _counter_dn);
	k_mutex_unlock(&counter_mutex);

	if (!ok)
	{
		LOG_ERR("CBOR: failed to encode actual state");
		return -ENOMEM;
	}

	size_t cbor_size = zse->payload - (const uint8_t *) cbor_buf;
	int err = -ENETDOWN;

	if (golioth_client_is_connected(client))
	{
		err = golioth_lightdb_set_async(client,
						APP_STATE_ACTUAL_PATH,
						GOLIOTH_CONTENT_TYPE_CBOR,
						cbor_buf,
						cbor_size,
						async_handler,
						NULL);

		if (err) {
			LOG_ERR("Unable to send actual state to LightDB State: %d", err);
		}
		else if (_initial_update_pending)
		{
			_initial_update_pending = false;
		}
	}
	return err;
}

/// Match a given string with ZCBOR decoded string
///
/// We don't know which order the key/value pairs will be in CBOR. This function takes a string to
/// compare, as well as both key/value pairs. It returns a pointer to the value whose key is a
/// match. If there is no match, NULL will be returned.
///
/// @param to_match String to match against the received key
/// @param zstr0    Pointer to first ZCBOR string
/// @param v0       Pointer to first ZCBOR value
/// @param zstr1    Pointer to second ZCBOR string
/// @param v1       Pointer to second ZCBOR value
///
/// @retval Pointer to int32_t value for the matched key or NULL
static int32_t *find_matching_pointer(const char *to_match, struct zcbor_string *zstr0, int32_t *v0,
				      struct zcbor_string *zstr1, int32_t *v1)
{
	if ((strlen(to_match) == zstr0->len) &&
	    (strncmp(to_match, zstr0->value, zstr0->len) == 0)) {
		return v0;
	}

	if ((strlen(to_match) == zstr1->len) && (strncmp(to_match, zstr1->value, zstr1->len) == 0))
	{
		return v1;
	}

	return NULL;
}

/// Validate and store received value
///
/// @param new_value Value received in CBOR packet
/// @param stored_value Pointer for storing validated value
/// @param str String associated with stored value for use in logging
static void process_desired_value(int32_t new_value, uint16_t *stored_value, const char *str)
{
	if (new_value == -1)
	{
		return;
	}

	if ((new_value < 0) || (new_value > COUNTER_MAX)) {
		LOG_ERR("'%s' server value out of bounds: %u (expected 0..%u)", str, new_value,
			COUNTER_MAX);
		return;
	}

	if (new_value == *stored_value)
	{
		LOG_INF("'%s' server value matches current: %u; ignoring.", str, new_value);

	}
	else
	{
		*stored_value = new_value;
		LOG_INF("Using new '%s' value from server: %u", str, *stored_value);
	}
}

static void app_state_desired_handler(struct golioth_client *client, enum golioth_status status,
				      const struct golioth_coap_rsp_code *coap_rsp_code,
				      const char *path, const uint8_t *payload, size_t payload_size,
				      void *arg)
{
	if (status != GOLIOTH_OK) {
		LOG_ERR("Failed to receive LightDB State '%s' path: %d", APP_STATE_DESIRED_PATH,
			status);
		return;
	}

	if ((payload_size == 1) && (payload[0] == 0xf6))
	{
		/* This is CBOR for NULL */
		LOG_WRN("'%s' path is NULL, resetting.", APP_STATE_DESIRED_PATH);
		goto reset_to_default;
	}

	ZCBOR_STATE_D(zsd, 2, payload, payload_size, 1, 0);

	struct zcbor_string key0;
	struct zcbor_string key1;
	int32_t value0;
	int32_t value1;

	int ok = zcbor_map_start_decode(zsd) &&
		 zcbor_tstr_decode(zsd, &key0) &&
		 zcbor_int32_decode(zsd, &value0) &&
		 zcbor_tstr_decode(zsd, &key1) &&
		 zcbor_int32_decode(zsd, &value1) &&
		 zcbor_map_end_decode(zsd);

	if (!ok)
	{
		LOG_ERR("Decoding failure, deleting path: '%s'", APP_STATE_DESIRED_PATH);
		golioth_lightdb_delete_async(client, APP_STATE_DESIRED_PATH, async_handler, NULL);
		return;
	}

	int32_t *up = find_matching_pointer(COUNTER_UP_STRING, &key0, &value0, &key1, &value1);
	int32_t *dn = find_matching_pointer(COUNTER_DN_STRING, &key0, &value0, &key1, &value1);

	if (!up || !dn)
	{
		LOG_ERR("Did not find expected keys, resetting '%s'", APP_STATE_DESIRED_PATH);
		goto reset_to_default;
	}

	if ((*up == -1) && (*dn == -1))
	{
		/* No changes hae been requested; do nothing */
		goto check_initial_update;
	}

	k_mutex_lock(&counter_mutex, K_FOREVER);
	process_desired_value(*up, &_counter_up, COUNTER_UP_STRING);
	process_desired_value(*dn, &_counter_dn, COUNTER_DN_STRING);
	k_mutex_unlock(&counter_mutex);

	app_state_update_actual();

reset_to_default:
	app_state_reset_desired();

check_initial_update:
	if (_initial_update_pending)
	{
		/* Ensure that actual state is sent at least once after power up */
		app_state_update_actual();
	}
}

int app_state_counter_change(void)
{
	k_mutex_lock(&counter_mutex, K_FOREVER);

	if (++_counter_up > COUNTER_MAX)
	{
		_counter_up = 0;
	}

	if (--_counter_dn > COUNTER_MAX)
	{
		_counter_dn = COUNTER_MAX;
	}

	LOG_DBG("D: %d; U: %d", _counter_dn, _counter_up);

	k_mutex_unlock(&counter_mutex);

	return app_state_update_actual();
}

int app_state_observe(struct golioth_client *state_client)
{
	client = state_client;

	int err = golioth_lightdb_observe_async(client,
						APP_STATE_DESIRED_PATH,
						GOLIOTH_CONTENT_TYPE_CBOR,
						app_state_desired_handler,
						NULL);
	if (err) {
		LOG_WRN("failed to observe lightdb path: %d", err);
		return err;
	}

	return err;
}
