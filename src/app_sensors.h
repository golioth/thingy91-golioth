/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SENSORS_H__
#define __APP_SENSORS_H__

#include <golioth/client.h>

void app_sensors_set_client(struct golioth_client *sensors_client);
void app_sensors_read_and_stream(void);

#endif /* __APP_SENSORS_H__ */
