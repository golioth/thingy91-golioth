/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_sensors, LOG_LEVEL_DBG);

#include <stdlib.h>
#include <golioth/client.h>
#include <golioth/stream.h>
#include <zcbor_encode.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>

#include "app_sensors.h"
#include "app_settings.h"

static struct golioth_client *client;

/* Sensor device structs */
#if defined(CONFIG_BOARD_THINGY91_NRF9160_NS)
const struct device *light = DEVICE_DT_GET_ONE(rohm_bh1749);
const struct device *weather = DEVICE_DT_GET_ONE(bosch_bme680);
const struct device *accel = DEVICE_DT_GET_ONE(adi_adxl362);
#endif

#if defined(CONFIG_BOARD_THINGY91X_NRF9151_NS)
#include <drivers/bme68x_iaq.h>
const struct device *weather = DEVICE_DT_GET_ONE(bosch_bme680);
const struct device *accel = DEVICE_DT_GET_ONE(adi_adxl367);
#endif

/* Callback for LightDB Stream */
static void async_error_handler(struct golioth_client *client, enum golioth_status status,
				const struct golioth_coap_rsp_code *coap_rsp_code, const char *path,
				void *arg)
{
	if (status != GOLIOTH_OK) {
		LOG_ERR("Async task failed: %d", status);
		return;
	}
}

static enum golioth_status read_light_sensor(zcbor_state_t *zse)
{
#if defined(CONFIG_DT_HAS_ROHM_BH1749_ENABLED)

	struct sensor_value BH1749_RED;
	struct sensor_value BH1749_GREEN;
	struct sensor_value BH1749_BLUE;
	struct sensor_value BH1749_IR;
	bool ok;

	/* Turn off LED so light sensor won't detect LED fade
	 * Also helps highlight that there is a reading being taken.
	 */
	all_leds_off();

	/* briefly sleep the thread to give time to run the LED thread */
	k_msleep(300);

	/* Start taking readings */

	/* BH1749 */
	int err = sensor_sample_fetch(light);

	all_leds_on();

	if (err) {
		LOG_ERR("Error fetching BH1749 sensor sample: %d", err);
		return GOLIOTH_ERR_FAIL;
	}
	sensor_channel_get(light, SENSOR_CHAN_RED, &BH1749_RED);
	sensor_channel_get(light, SENSOR_CHAN_GREEN, &BH1749_GREEN);
	sensor_channel_get(light, SENSOR_CHAN_BLUE, &BH1749_BLUE);
	sensor_channel_get(light, SENSOR_CHAN_IR, &BH1749_IR);
	LOG_DBG("R: %d, G: %d, B: %d, IR: %d", BH1749_RED.val1, BH1749_GREEN.val1, BH1749_BLUE.val1,
		BH1749_IR.val1);

	ok = zcbor_tstr_put_lit(zse, "light") && zcbor_map_start_encode(zse, 4);
	if (!ok)
	{
		LOG_ERR("ZCBOR unable to open light map");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	ok = zcbor_tstr_put_lit(zse, "red") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&BH1749_RED)) &&
	     zcbor_tstr_put_lit(zse, "green") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&BH1749_GREEN)) &&
	     zcbor_tstr_put_lit(zse, "blue") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&BH1749_BLUE)) &&
	     zcbor_tstr_put_lit(zse, "ir") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&BH1749_IR));
	if (!ok)
	{
		LOG_ERR("ZCBOR failed to encode light data");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	ok = zcbor_map_end_encode(zse, 4);
	if (!ok)
	{
		LOG_ERR("ZCBOR failed to close light map");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	return GOLIOTH_OK;

#else

	return GOLIOTH_OK;

#endif /* CONFIG_DT_HAS_ROHM_BH1749_ENABLED */
}


/* This function assume that sensor_sample_fetch() has already been run */
static enum golioth_status process_gas_sensor(zcbor_state_t *zse)
{
#if defined(CONFIG_BOARD_THINGY91_NRF9160_NS)

	struct sensor_value gas_res;
	bool ok;

	sensor_channel_get(weather, SENSOR_CHAN_GAS_RES, &gas_res);
	LOG_DBG("G: %d.%06d", gas_res.val1, gas_res.val2);

	ok = zcbor_tstr_put_lit(zse, "gas") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&gas_res));

	if (!ok)
	{
		LOG_ERR("ZCBOR unable to encode gas data");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	return GOLIOTH_OK;

#elif defined(CONFIG_BOARD_THINGY91X_NRF9151_NS)

	struct sensor_value gas_iaq = { 0 };
	struct sensor_value gas_co2 = { 0 };
	struct sensor_value gas_voc = { 0 };
	bool ok;

	sensor_channel_get(weather, SENSOR_CHAN_IAQ, &gas_iaq);
	sensor_channel_get(weather, SENSOR_CHAN_CO2, &gas_co2);
	sensor_channel_get(weather, SENSOR_CHAN_VOC, &gas_voc);
	LOG_DBG("IAQ: %d; CO2: %d.%06d; VOC: %d.%06d",
		gas_iaq.val1, gas_co2.val1, gas_co2.val2, gas_voc.val1, gas_voc.val2);

	ok = zcbor_tstr_put_lit(zse, "iaq") &&
	     zcbor_int32_put(zse, gas_iaq.val1) &&
	     zcbor_tstr_put_lit(zse, "co2") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&gas_co2)) &&
	     zcbor_tstr_put_lit(zse, "voc") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&gas_voc));

	if (!ok)
	{
		LOG_ERR("ZCBOR unable to encode gas data");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	return GOLIOTH_OK;

#else

	return GOLIOTH_OK;

#endif
}
static enum golioth_status read_weather_sensor(zcbor_state_t *zse)
{
	struct sensor_value temp;
	struct sensor_value press;
	struct sensor_value humidity;
	bool ok;
	enum golioth_status status;

	/* BME680 */
	int err = sensor_sample_fetch(weather);
	if (err) {
		LOG_ERR("Error fetching BME680 sensor sample: %d", err);
		return GOLIOTH_ERR_FAIL;
	}
	sensor_channel_get(weather, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(weather, SENSOR_CHAN_PRESS, &press);
	sensor_channel_get(weather, SENSOR_CHAN_HUMIDITY, &humidity);
	LOG_DBG("T: %d.%06d; P: %d.%06d; H: %d.%06d", temp.val1, abs(temp.val2), press.val1,
		press.val2, humidity.val1, humidity.val2);

	ok = zcbor_tstr_put_lit(zse, "weather") && zcbor_map_start_encode(zse, 4);
	if (!ok)
	{
		LOG_ERR("ZCBOR unable to open weather map");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	ok = zcbor_tstr_put_lit(zse, "tem") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&temp)) &&
	     zcbor_tstr_put_lit(zse, "pre") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&press)) &&
	     zcbor_tstr_put_lit(zse, "hum") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&humidity));
	if (!ok)
	{
		LOG_ERR("ZCBOR failed to encode weather data");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	status = process_gas_sensor(zse);
	if (status != GOLIOTH_OK)
	{
		return status;
	}

	ok = zcbor_map_end_encode(zse, 4);
	if (!ok)
	{
		LOG_ERR("ZCBOR failed to close weather map");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	return GOLIOTH_OK;
}

static enum golioth_status read_accel_sensor(zcbor_state_t *zse)
{
	struct sensor_value accel_x;
	struct sensor_value accel_y;
	struct sensor_value accel_z;
	bool ok;

	/* ADXL362 */
	int err = sensor_sample_fetch(accel);
	if (err) {
		LOG_ERR("Error fetching ADXL362 sensor sample: %d", err);
		return GOLIOTH_ERR_FAIL;
	}
	sensor_channel_get(accel, SENSOR_CHAN_ACCEL_X, &accel_x);
	sensor_channel_get(accel, SENSOR_CHAN_ACCEL_Y, &accel_y);
	sensor_channel_get(accel, SENSOR_CHAN_ACCEL_Z, &accel_z);
	LOG_DBG("X: %d.%06d; Y: %d.%06d; Z: %d.%06d", accel_x.val1, abs(accel_x.val2), accel_y.val1,
		abs(accel_y.val2), accel_z.val1, abs(accel_z.val2));

	ok = zcbor_tstr_put_lit(zse, "accel") && zcbor_map_start_encode(zse, 3);
	if (!ok)
	{
		LOG_ERR("ZCBOR unable to open accel map");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	ok = zcbor_tstr_put_lit(zse, "x") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&accel_x)) &&
	     zcbor_tstr_put_lit(zse, "y") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&accel_y)) &&
	     zcbor_tstr_put_lit(zse, "z") &&
	     zcbor_float64_put(zse, sensor_value_to_double(&accel_z));
	if (!ok)
	{
		LOG_ERR("ZCBOR failed to encode accel data");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	ok = zcbor_map_end_encode(zse, 3);
	if (!ok)
	{
		LOG_ERR("ZCBOR failed to close accel map");
		return GOLIOTH_ERR_QUEUE_FULL;
	}

	return GOLIOTH_OK;
}

/* This will be called by the main() loop after delays or on button presses */
/* Do all of your work here! */
void app_sensors_read_and_stream(void)
{
	int err;
	bool ok;
	enum golioth_status status;
	char cbor_buf[256];

	ZCBOR_STATE_E(zse, 3, cbor_buf, sizeof(cbor_buf), 1);

	ok = zcbor_map_start_encode(zse, 3);

	if (!ok)
	{
		LOG_ERR("ZCBOR failed to open map");
		return;
	}

	status = read_light_sensor(zse);
	if (status == GOLIOTH_ERR_QUEUE_FULL)
	{
		return;
	}

	status = read_weather_sensor(zse);
	if (status == GOLIOTH_ERR_QUEUE_FULL)
	{
		return;
	}

	status = read_accel_sensor(zse);
	if (status == GOLIOTH_ERR_QUEUE_FULL)
	{
		return;
	}

	ok = zcbor_map_end_encode(zse, 3);
	if (!ok)
	{
		LOG_ERR("ZCBOR failed to close map");
	}

	size_t cbor_size = zse->payload - (const uint8_t *) cbor_buf;

	/* Only stream sensor data if connected */
	if (golioth_client_is_connected(client)) {
		/* Send to LightDB Stream on "sensor" endpoint */
		err = golioth_stream_set_async(client, "sensor", GOLIOTH_CONTENT_TYPE_CBOR, cbor_buf,
					cbor_size, async_error_handler, NULL);
		if (err) {
			LOG_ERR("Failed to send sensor data to Golioth: %d", err);
		}
	} else {
		LOG_DBG("No connection available, skipping sending data to Golioth");
	}
}

void app_sensors_set_client(struct golioth_client *sensors_client)
{
	client = sensors_client;
}
